#if defined _smtester_included
	#endinput
#endif
#define _smtester_included

#define SMTESTER_VERSION "1.0.0"

#include <profiler>

static bool g_ExitOnFinish;
static bool g_ShouldFinishOnResolve;

static ArrayList g_Tasks; 
static StringMap g_AsyncTasks;
static Profiler g_Profiler;

enum SMTesterTaskType
{
	TaskType_Node,
	TaskType_GoBack,
	TaskType_Assert,
	TaskType_Async_Pending,
	TaskType_Async_TimedOut,
	TaskType_Async_Finished,
}

enum struct SMTesterTask
{
	SMTesterTaskType type;
	char name[64];
	
	bool should_fail;
	any expect;
	any result;
	
	float timeout;
	char reject_reason[256];
}

/*
 * Starts testing. 
 * The test result (0 on success, 1 on fail) will be dumped 
 * in sourcemod/logs/test_result.txt if exit_on_finish is true.
 *
 * Must be finished with SMTester_Finish().
 *
 * @param exit_on_finish        Whether the server should be exited on test end.
 */
stock void SMTester_Start(bool exit_on_finish = false)
{
	g_ExitOnFinish = exit_on_finish;
	
	g_Tasks = new ArrayList(sizeof(SMTesterTask));
	g_AsyncTasks = new StringMap();
	
	g_Profiler = new Profiler();
	g_Profiler.Start();
}

/* 
 * Creates a new testing category. 
 */
stock void SMTester_CreateNode(const char[] name)
{
	CheckIfStarted();
	
	SMTesterTask task;
	task.type = TaskType_Node;
	strcopy(task.name, sizeof(task.name), name);
	
	g_Tasks.PushArray(task);
}

/* 
 * Goes back to the previous category. 
 */
stock bool SMTester_GoBack(int times = 1)
{
	CheckIfStarted();
	
	SMTesterTask task;
	task.type = TaskType_GoBack;
	task.result = times;
	
	g_Tasks.PushArray(task);
}

/* 
 * Performs an assert. 
 */
stock void SMTester_Assert(const char[] name, any value, any expect = true, const char[] reject_reason = "", any ...)
{
	CheckIfStarted();
	
	SMTesterTask task;
	task.type = TaskType_Assert;
	task.expect = expect;
	task.result = value;
	strcopy(task.name, sizeof(task.name), name);
	
	char reject[256];
	VFormat(reject, sizeof(reject), reject_reason, 5);
	strcopy(task.reject_reason, sizeof(task.reject_reason), reject);
	
	g_Tasks.PushArray(task);
}

/* 
 * Performs an assert that should fail. 
 */
stock void SMTester_AssertFail(const char[] name, any value, any expect = true, const char[] reject_reason = "", any ...)
{
	CheckIfStarted();
	
	SMTesterTask task;
	task.type = TaskType_Assert;
	task.expect = expect;
	task.result = value;
	task.should_fail = true;
	strcopy(task.name, sizeof(task.name), name);
	
	char reject[256];
	VFormat(reject, sizeof(reject), reject_reason, 5);
	strcopy(task.reject_reason, sizeof(task.reject_reason), reject);
	
	g_Tasks.PushArray(task);
}

/* 
 * Starts an asynchronous assert. Name must be unique. 
 */
stock void SMTester_Async(const char[] name, any expect = true, float timeout = 120.0)
{
	CheckIfStarted();
	
	SMTesterTask task;
	task.type = TaskType_Async_Pending;
	strcopy(task.name, sizeof(task.name), name);
	task.expect = expect;
	task.timeout = timeout;
	
	int index = g_Tasks.PushArray(task);
	g_AsyncTasks.SetValue(name, index);
	CreateTimer(timeout, SMTester_Timer_AsyncTimeout, index);
}

/* 
 * Starts an asynchronous assert that should fail. Name must be unique. 
 */
stock void SMTester_AsyncFail(const char[] name, any expect = true, float timeout = 120.0)
{
	CheckIfStarted();
	
	SMTesterTask task;
	task.type = TaskType_Async_Pending;
	strcopy(task.name, sizeof(task.name), name);
	task.expect = expect;
	task.timeout = timeout;
	task.should_fail = true;
	
	int index = g_Tasks.PushArray(task);
	g_AsyncTasks.SetValue(name, index);
	CreateTimer(timeout, SMTester_Timer_AsyncTimeout, index);
}

/* 
 * Finishes an asynchronous assert.
 */
stock void SMTester_AsyncAssert(const char[] name, any result, const char[] reject_reason = "", any ...)
{
	if (g_AsyncTasks == null) {
		return;
	}
	
	int index;
	if (g_AsyncTasks.GetValue(name, index)) {
		SMTesterTask task;
		g_Tasks.GetArray(index, task);
		task.type = TaskType_Async_Finished;
		task.result = result;
		
		char reject[256];
		VFormat(reject, sizeof(reject), reject_reason, 4);
		strcopy(task.reject_reason, sizeof(task.reject_reason), reject);
		
		g_Tasks.SetArray(index, task);
		g_AsyncTasks.Remove(name);
		
		CheckTasks();
	}
}

stock void SMTester_Finish()
{
	CheckIfStarted();
	
	int tests;
	int fails;
	int currentnode = -1;
	
	if (!g_ShouldFinishOnResolve) {
		int len = g_Tasks.Length;
		for (int i = 0; i < len; i ++) {
			SMTesterTask task;
			g_Tasks.GetArray(i, task);
			
			if (task.type == TaskType_Async_Pending) {
				g_ShouldFinishOnResolve = true;
				return;
			}
		}
	}
	
	PrintToServer("\nSMTester %s", SMTESTER_VERSION);
	
	int len = g_Tasks.Length;
	for (int i = 0; i < len; i ++) {
		SMTesterTask task;
		g_Tasks.GetArray(i, task);
		
		switch (task.type) {
			case (TaskType_Node): {
				++currentnode;
				PrintToServer("%s%s", GetPadding(currentnode), task.name);
				++currentnode;
			}
			
			case (TaskType_GoBack): {
				currentnode -= task.result*2;
			}
			
			case (TaskType_Assert): {
				++tests;
				
				if (task.result != task.expect) {
					if (!task.should_fail) {
						++fails;
					}
					
					char reject_msg[256];
					if (task.reject_reason[0] != '\0') {
						strcopy(reject_msg, sizeof(reject_msg), task.reject_reason);
					} else {
						FormatEx(reject_msg, sizeof(reject_msg), "Expected %i, got %i", task.expect, task.result);
					}
					
					PrintToServer("%s(-) %s: %s", GetPadding(currentnode), task.name, reject_msg);
				} else {
					PrintToServer("%s(+) %s", GetPadding(currentnode), task.name);
				}
			}
			
			case (TaskType_Async_TimedOut): {
				++tests;
				
				if (!task.should_fail) {
					++fails;
				}
				
				PrintToServer("%s(-) %s: Timed out after %.1fs", GetPadding(currentnode), task.name, task.timeout);
			}
			
			case (TaskType_Async_Finished): {
				++tests;
				
				if (task.result != task.expect) {
					if (!task.should_fail) {
						++fails;
					}
					
					char reject_msg[64];
					if (task.reject_reason[0] != '\0') {
						strcopy(reject_msg, sizeof(reject_msg), task.reject_reason);
					} else {
						FormatEx(reject_msg, sizeof(reject_msg), "Expected %i, got %i", task.expect, task.result);
					}
					
					PrintToServer("%s(-) %s: %s", GetPadding(currentnode), task.name, reject_msg);
				} else {
					PrintToServer("%s(+) %s", GetPadding(currentnode), task.name);
				}
			}
		}
	}
	
	g_Profiler.Stop();
	
	PrintToServer("\n%i passing (%ims)", tests - fails, RoundFloat(g_Profiler.Time*1000.0));
	
	if (fails) {
		PrintToServer("%i failing", fails);
	}
	
	PrintToServer(" ");
	
	if (fails) {
		Exit(1);
		SetFailState("%i Failed %s", fails, fails == 1 ? "Test" : "Tests");
	} else {
		Exit(0);
	}
	
	delete g_Tasks;
	delete g_AsyncTasks;
	delete g_Profiler;
}

static stock void CheckIfStarted()
{
	if (g_Tasks == null) {
		ThrowError("SMTester wasn't started!");
	}
}

static stock char GetPadding(int nodes)
{
	char padding[256];
	
	for (int i = 0; i < nodes; i++) {
		Format(padding, sizeof(padding), "  %s", padding);
	}
	
	return padding;
}

// Lame workaround because i can't exit srcds with code 0 through sm
static stock void Exit(int code)
{
	if (g_ExitOnFinish) {
		char path[PLATFORM_MAX_PATH];
		BuildPath(Path_SM, path, sizeof(path), "/logs/test_result.txt");
		
		File file = OpenFile(path, "w");
		char codestr[16];
		IntToString(code, codestr, sizeof(codestr));
		file.WriteString(codestr, false);
		delete file;
		
		ServerCommand("exit");
	}
}

static stock void CheckTasks()
{
	if (g_ShouldFinishOnResolve) {
		int len = g_Tasks.Length;
		for (int i = 0; i < len; i ++) {
			SMTesterTask task;
			g_Tasks.GetArray(i, task);
			
			// If we still have pending tasks, finish when all of them are finished/timed out
			if (task.type == TaskType_Async_Pending) {
				return;
			}
		}
		SMTester_Finish();
	}
}

public Action SMTester_Timer_AsyncTimeout(Handle timer, int index)
{
	if (g_Tasks != null) {
		SMTesterTask task;
		g_Tasks.GetArray(index, task);
		if (task.type != TaskType_Async_Finished) {
			task.type = TaskType_Async_TimedOut;
			g_Tasks.SetArray(index, task);
			
			CheckTasks();
		}
	}
}