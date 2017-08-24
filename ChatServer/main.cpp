#include "SocketSession.h"
#include "lua_engine.h"
#include "fxtimer.h"
#include "fxdb.h"
#include "fxmeta.h"

#include <signal.h>

unsigned int g_dwPort = 12000;
unsigned int g_dwChatSessionPort = 20000;
unsigned int g_dwChatServerSessionPort = 20001;
bool g_bRun = true;

void EndFun(int n)
{
	if (n == SIGINT || n == SIGTERM)
	{
		g_bRun = false;
	}
	else
	{
		printf("unknown signal : %d !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n", n);
	}
}

int main(int argc, char **argv)
{
	//----------------------order can't change begin-----------------------//
	signal(SIGINT, EndFun);
	signal(SIGTERM, EndFun);
	if (!LogThread::CreateInstance())
	{
		return 0;
	}

	if (!CLuaEngine::CreateInstance())
	{
		return 0;
	}
	std::vector<ToluaFunctionOpen*> vecFunctions;
	int tolua_LuaMeta_open(lua_State*);
	vecFunctions.push_back(tolua_LuaMeta_open);
	if (!CLuaEngine::Instance()->Init(vecFunctions))
	{
		return 0;
	}
	if (!CLuaEngine::Instance()->Reload(WORK_PATH))
	{
		return 0;
	}
	// must define before goto
	IFxNet* pNet = NULL;
	IFxListenSocket* pListenSocket = NULL;

	if (!CLuaEngine::Instance()->CommandLineFunction(argv, argc))
	{
		return 0;
	}
	if (!GetTimeHandler()->Init())
	{
		return 0;
	}
	GetTimeHandler()->Run();
	if (!LogThread::Instance()->Init())
	{
		g_bRun = false;
		goto STOP;
	}

	if (!CSessionFactory::CreateInstance())
	{
		g_bRun = false;
		goto STOP;
	}
	CSessionFactory::Instance()->Init();
	pNet = FxNetGetModule();
	if (!pNet)
	{
		g_bRun = false;
		goto STOP;
	}
	//----------------------order can't change end-----------------------//

	//SDBConnInfo oInfo;
	//memset(&oInfo, 0, sizeof(oInfo));
	//oInfo.m_dwDBId = 0;
	//strcpy(oInfo.m_stAccount.m_szCharactSet, "utf8");
	//strcpy(oInfo.m_stAccount.m_szDBName, "chat");
	//strcpy(oInfo.m_stAccount.m_szHostName, "127.0.0.1");
	//strcpy(oInfo.m_stAccount.m_szLoginName, "test");
	//strcpy(oInfo.m_stAccount.m_szLoginPwd, "test");
	//oInfo.m_stAccount.m_wConnPort = 3306;
	//if (FxDBGetModule()->Open(oInfo))
	//{
	//	LogFun(LT_Screen, LogLv_Info, "%s", "db connected~~~~");
	//}

	pListenSocket = pNet->Listen(CSessionFactory::Instance(), SLT_CommonTcp, 0, g_dwPort);
	if(pListenSocket == NULL)
	{
		g_bRun = false;
		goto STOP;
	}
	while (g_bRun)
	{
		GetTimeHandler()->Run();
		pNet->Run(0xffffffff);
		//LogFun(LT_Screen, LogLv_Info, "%s", PrintTrace());
		FxSleep(1);
	}
	pListenSocket->StopListen();
	pListenSocket->Close();
	for (std::set<FxSession*>::iterator it = CSessionFactory::Instance()->m_setSessions.begin();
		it != CSessionFactory::Instance()->m_setSessions.end(); ++it)
	{
		(*it)->Close();
	}

	while (CSessionFactory::Instance()->m_setSessions.size())
	{
		pNet->Run(0xffffffff);
		FxSleep(10);
	}
	FxSleep(10);
	pNet->Release();
STOP:
	LogThread::Instance()->Stop();
}