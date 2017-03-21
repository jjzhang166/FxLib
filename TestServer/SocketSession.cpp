#include "SocketSession.h"
#include <string>
#include <stdio.h>
#include "fxdb.h"
#include "netstream.h"

CSocketSession::CSocketSession()
{
}


CSocketSession::~CSocketSession()
{
}

void CSocketSession::OnConnect(void)
{
//	LogFun(LT_Screen, LogLv_Debug, "ip : %s, port : %d", GetRemoteIPStr(), GetRemotePort());
}

void CSocketSession::OnClose(void)
{
//	LogFun(LT_Screen, LogLv_Debug, "ip : %s, port : %d", GetRemoteIPStr(), GetRemotePort());
}

void CSocketSession::OnError(UINT32 dwErrorNo)
{
	LogFun(LT_Screen | LT_File, LogLv_Debug, "ip : %s, port : %d, connect addr : %p, error no : %d", GetRemoteIPStr(), GetRemotePort(), (GetConnection()), dwErrorNo);
}

class DBQuery : public IQuery
{
public:
	DBQuery(){}
	virtual ~DBQuery(){}

	virtual INT32 GetDBId(void) { return 0; }

	virtual void OnQuery(IDBConnection *poDBConnection)
	{
		IDataReader* pReader = NULL;
		if (poDBConnection->Query(m_strQuery.c_str(), &pReader) == FXDB_HAS_RESULT)
		{
			while(pReader->GetNextRecord())
			{
				char strValue[1024] = { 0 };
				UINT32 dwLen = 0;
				dwLen += sprintf(strValue + dwLen, "%s", "role id : ");
				dwLen += sprintf(strValue + dwLen, "%s", pReader->GetFieldValue(0));
				dwLen += sprintf(strValue + dwLen, "%s", " user id : ");
				dwLen += sprintf(strValue + dwLen, "%s", pReader->GetFieldValue(1));
				dwLen += sprintf(strValue + dwLen, "%s", " role name ");
				dwLen += sprintf(strValue + dwLen, "%s", pReader->GetFieldValue(2));
				dwLen += sprintf(strValue + dwLen, "%s", " clan ");
				dwLen += sprintf(strValue + dwLen, "%s", pReader->GetFieldValue(3));

				pSession->Send(strValue, 1024);
			}
			pReader->Release();
		}
	}

	virtual void OnResult(void)
	{}

	virtual void Release(void)
	{
		delete this;
	}

	std::string m_strQuery;
	CSocketSession* pSession;

private:

};

void CSocketSession::OnRecv(const char* pBuf, UINT32 dwLen)
{
	LogExe(LogLv_Debug, "ip : %s, port : %d, recv %s", GetRemoteIPStr(), GetRemotePort(), pBuf);
	//printf("time : %s ip : %s, port : %d, recv %s", GetTimeHandler()->GetTimeStr(), GetRemoteIPStr(), GetRemotePort(), pBuf);

	if (!Send(pBuf, dwLen))
	{
		LogFun(LT_Screen | LT_File, LogLv_Debug, "ip : %s, port : %d, recv %s send error", GetRemoteIPStr(), GetRemotePort(), pBuf);
		Close();
	}

	//DBQuery * pQuery = new DBQuery;
	//pQuery->m_strQuery = pBuf;
	//FxDBGetModule()->AddQuery(pQuery);
	//pQuery->pSession = this;
}

void CSocketSession::Release(void)
{
	LogFun(LT_Screen, LogLv_Debug, "ip : %s, port : %d, connect addr : %p", GetRemoteIPStr(), GetRemotePort(), GetConnection());
	OnDestroy();

	Init(NULL);

	m_pFxSessionFactory->Release(this);
}

IMPLEMENT_SINGLETON(CSessionFactory)

CSessionFactory::CSessionFactory()
{
	m_pLock = FxCreateThreadLock();
	for(int i = 0; i < 64; ++i)
	{
		CSocketSession* pSession = new CSocketSession;
		pSession->SetDataHeader(oDataHeaderFactory.CreateDataHeader());
		pSession->SetFxSessionFactory(this);
		m_listSession.push_back(pSession);
	}
//	m_poolSessions.Init(100, 10, false, 100);
}

FxSession*	CSessionFactory::CreateSession()
{
	m_pLock->Lock();
	FxSession* pSession = NULL;
	if(m_listSession.size() > 0)
	{
		pSession = *(m_listSession.begin());
		m_listSession.pop_front();
	}
	if(pSession)
	{
		if(pSession->GetDataHeader() == NULL)
		{
			pSession->SetDataHeader(oDataHeaderFactory.CreateDataHeader());
		}

		m_setSessions.insert(pSession);
	}
	LogFun(LT_Screen | LT_File, LogLv_Debug, "left free session : %d", (int)m_listSession.size());
	m_pLock->UnLock();
	return pSession;
}

void CSessionFactory::Release(FxSession* pSession)
{
	m_pLock->Lock();
//	m_poolSessions.ReleaseObj(pSession);
	m_listSession.push_back(pSession);
	m_setSessions.erase(pSession);
	LogFun(LT_Screen | LT_File, LogLv_Debug, "left free session : %d", (int)m_listSession.size());
	m_pLock->UnLock();
}

IMPLEMENT_SINGLETON(CWebSocketSessionFactory)

CWebSocketSessionFactory::CWebSocketSessionFactory()
{
	m_pLock = FxCreateThreadLock();
	for (int i = 0; i < 64; ++i)
	{
		CSocketSession* pSession = new CSocketSession;
		pSession->SetDataHeader(oWebSocketDataHeaderFactory.CreateDataHeader());
		pSession->SetFxSessionFactory(this);
		m_listSession.push_back(pSession);
	}
}

FxSession * CWebSocketSessionFactory::CreateSession()
{
	m_pLock->Lock();
	FxSession* pSession = NULL;
	if (m_listSession.size() > 0)
	{
		pSession = *(m_listSession.begin());
		m_listSession.pop_front();
	}
	if (pSession)
	{
		if (pSession->GetDataHeader() == NULL)
		{
			pSession->SetDataHeader(oWebSocketDataHeaderFactory.CreateDataHeader());
		}

		m_setSessions.insert(pSession);
	}
	LogFun(LT_Screen | LT_File, LogLv_Debug, "left free session : %d", (int)m_listSession.size());
	m_pLock->UnLock();
	return pSession;
}

void CWebSocketSessionFactory::Release(FxSession * pSession)
{
	m_pLock->Lock();
	//	m_poolSessions.ReleaseObj(pSession);
	m_listSession.push_back(pSession);
	m_setSessions.erase(pSession);
	LogFun(LT_Screen | LT_File, LogLv_Debug, "left free session : %d", (int)m_listSession.size());
	m_pLock->UnLock();
}

DataHeader::DataHeader()
{
}

DataHeader::~DataHeader()
{

}

void* DataHeader::GetPkgHeader()
{
	return (void*)m_dataRecvBuffer;
}

void* DataHeader::BuildSendPkgHeader(UINT32& dwHeaderLen, UINT32 dwDataLen)
{
	//*((UINT32*)m_dataBuffer) = htonl(dwDataLen);
	dwHeaderLen = sizeof(m_dataSendBuffer);
	CNetStream oNetStream(ENetStreamType_Write, m_dataSendBuffer, sizeof(m_dataSendBuffer));
	oNetStream.WriteInt(dwDataLen);
	oNetStream.WriteInt(s_dwMagic);
	return (void*)m_dataSendBuffer;
}

bool DataHeader::BuildRecvPkgHeader(char* pBuff, UINT32 dwLen, UINT32 dwOffset)
{
	if (dwLen + dwOffset > GetHeaderLength())
	{
		return false;
	}

	memcpy(m_dataRecvBuffer + dwOffset, pBuff, dwLen);
	return true;
}

int DataHeader::__CheckPkgHeader(const char* pBuf)
{
	CNetStream oHeaderStream(m_dataRecvBuffer, sizeof(m_dataRecvBuffer));
	CNetStream oRecvStream(pBuf, sizeof(m_dataRecvBuffer));

	UINT32 dwHeaderLength = 0;
	UINT32 dwBufferLength = 0;
	oHeaderStream.ReadInt(dwHeaderLength);
	oRecvStream.ReadInt(dwBufferLength);

	UINT32 dwHeaderMagic = 0;
	UINT32 dwBufferMagic = 0;
	oHeaderStream.ReadInt(dwHeaderMagic);
	oRecvStream.ReadInt(dwBufferMagic);

	if (dwHeaderLength != dwBufferLength)
	{
		return -1;
	}

	if (s_dwMagic != dwBufferMagic)
	{
		return -1;
	}

	return (GetHeaderLength() + dwHeaderLength);
}

WebSocketDataHeader::WebSocketDataHeader()
	:m_dwHeaderLength(0)
{
}

WebSocketDataHeader::~WebSocketDataHeader()
{
}

void * WebSocketDataHeader::GetPkgHeader()
{
	return (void*)m_dataRecvBuffer;
}

void* WebSocketDataHeader::BuildSendPkgHeader(UINT32& dwHeaderLen, UINT32 dwDataLen)
{
	dwHeaderLen = 2;
	CNetStream oNetStream(ENetStreamType_Write, m_dataSendBuffer, sizeof(m_dataSendBuffer));
	unsigned char ucFinOpCode = 0x82;
	unsigned char ucFin = (ucFinOpCode >> 7) & 0xff;
	unsigned char ucOpCode = (ucFinOpCode) & 0x0f;
	oNetStream.WriteByte(ucFinOpCode);
	if (dwDataLen <= 126)
	{
		unsigned char ucLen = dwDataLen;
		oNetStream.WriteByte(ucLen);
	}
	else if (dwDataLen > 126 && dwDataLen < 0xFFFF)
	{
		unsigned char ucLen = 126;
		oNetStream.WriteByte(ucLen);
		dwHeaderLen += 2;
		unsigned short wLen = dwDataLen;
		oNetStream.WriteShort(wLen);
	}
	else
	{
		unsigned char ucLen = 127;
		oNetStream.WriteByte(ucLen);
		dwHeaderLen += 8;
		unsigned long long ullLen = dwDataLen;
		oNetStream.WriteInt64(ullLen);
	}
	return (void*)m_dataSendBuffer;
}

bool WebSocketDataHeader::BuildRecvPkgHeader(char * pBuff, UINT32 dwLen, UINT32 dwOffset)
{
	memcpy(m_dataRecvBuffer + dwOffset, pBuff, sizeof(m_dataRecvBuffer) - dwOffset > dwLen ? dwLen : sizeof(m_dataRecvBuffer) - dwOffset);
	m_dwHeaderLength = 0;
	if (dwLen + dwOffset > 2)
	{
		__CheckPkgHeader(NULL);
	}
	return true;
}

int WebSocketDataHeader::__CheckPkgHeader(const char * pBuf)
{
	m_dwHeaderLength = 0;
	CNetStream oHeaderStream(m_dataRecvBuffer, sizeof(m_dataRecvBuffer));
	
	unsigned char uc1, uc2;
	oHeaderStream.ReadByte(uc1);
	oHeaderStream.ReadByte(uc2);
	m_dwHeaderLength += 2;
	m_ucFin = (uc1 >> 7) & 0xff;
	m_ucOpCode = (uc1) & 0x0f;
	m_ucMask = (uc2 >> 7) & 0xff;
	m_ullPayloadLen = uc2 & 0x7f;

	if (m_ullPayloadLen == 126)
	{
		unsigned short wTemp = 0;
		oHeaderStream.ReadShort(wTemp);
		m_ullPayloadLen = wTemp;
		m_dwHeaderLength += 2;
	}
	else if (m_ullPayloadLen == 127)
	{
		unsigned long long ullTemp = 0;
		oHeaderStream.ReadInt64(ullTemp);
		m_ullPayloadLen = ullTemp;
		m_dwHeaderLength += 8;
	}
	
	if (m_ucMask)
	{
		memcpy(m_ucMaskingKey, oHeaderStream.ReadData(sizeof(m_ucMaskingKey)), sizeof(m_ucMaskingKey));
		m_dwHeaderLength += 4;
	}

	return (int)(m_ullPayloadLen + m_dwHeaderLength);
}

int WebSocketDataHeader::ParsePacket(const char * pBuf, UINT32 dwLen)
{
	if (dwLen < 2)
	{
		return 0;
	}

	int nPkgLen = __CheckPkgHeader(NULL);

	if (dwLen < m_dwHeaderLength)
	{
		return 0;
	}

	return nPkgLen;
}
