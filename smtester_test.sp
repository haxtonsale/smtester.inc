#include <sourcemod>
#include <smtester>

public void OnPluginStart()
{
	SMTester_Start();
	
	SMTester_CreateNode("Sync");
	
	SMTester_CreateNode("Should Pass");
	SMTester_Assert("1 == 1", 1 == 1);
	SMTester_Assert("2 + 2 == 4", 2 + 2, 4);
	SMTester_GoBack();
	
	SMTester_CreateNode("Should Fail");
	SMTester_AssertFail("1 == 2", 1 == 2);
	SMTester_AssertFail("2 + 2 == 3", 2 + 2, 3);
	SMTester_AssertFail("Custom reject reason", 2, 3, "Custom reject reason with %s", "formatting");
	
	SMTester_GoBack(2);
	SMTester_CreateNode("Async");
	
	SMTester_CreateNode("Should Pass");
	SMTester_Async("1 == 1");
	SMTester_Async("2 + 2 == 4", 4);
	SMTester_GoBack();
	
	SMTester_CreateNode("Should Fail");
	SMTester_AsyncFail("1 == 2");
	SMTester_AsyncFail("2 + 2 == 3", 3);
	SMTester_AsyncFail("Custom reject reason");
	SMTester_AsyncFail("Should be timed out", _, 6.0);
	
	SMTester_Finish();
	
	CreateTimer(5.0, Timer_AsyncAssert);
}

public Action Timer_AsyncAssert(Handle timer)
{
	SMTester_AsyncAssert("1 == 1", 1 == 1);
	SMTester_AsyncAssert("2 + 2 == 4", 4);
	SMTester_AsyncAssert("1 == 2", 1 == 2);
	SMTester_AsyncAssert("2 + 2 == 3", 2 + 2);
	SMTester_AsyncAssert("Custom reject reason", false, "Custom reject reason with %s", "epic formatting");
}