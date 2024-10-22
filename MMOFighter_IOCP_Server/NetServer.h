#ifndef __NETSERVER_CLASS__
#define __NETSERVER_CLASS__

#include "PCH.h"

// ������ ��� ��� Ŭ����
class NetServer
{
public:
	NetServer();

	~NetServer();

	// ���� ���� ����
	bool DisconnectSession(uint32_t sessionID);

	// ��Ŷ ����
	bool SendPacket(uint32_t sessionID, CPacket* packet);

	// ��Ŷ ���� - ���� �� ����
	bool SendPacketAndDisconnect(uint32_t sessionID, CPacket* packet, DWORD timeout = 0);

	// Server Start
	bool Start(const wchar_t* IP, unsigned short PORT, int createWorkerThreadCnt, int runningWorkerThreadCnt, bool nagelOff, bool zeroCopyOff, int maxAcceptCnt, unsigned char packet_code, unsigned char packet_key, DWORD timeout);

	// Server Stop
	void Stop();

protected:
	// ==========================================================
	// White IP Check 
	// [PARAM] const wchar_t* IP, unsigned short PORT
	// [RETURN] TRUE : ���� ��� / FALSE : ���� �ź�
	// ==========================================================
	virtual bool OnConnectionRequest(const wchar_t* IP, unsigned short PORT) = 0;

	// ==========================================================
	// ����ó�� �Ϸ� �� ȣ�� 
	// [PARAM] __int64 sessionID
	// [RETURN] X
	// ==========================================================
	virtual void OnClientJoin(uint32_t sessionID) = 0;

	// ==========================================================
	// �������� �� ȣ��, Player ���� ���ҽ� ����
	// [PARAM] __int64 sessionID
	// [RETURN] X 
	// ==========================================================
	virtual void OnClientLeave(uint32_t sessionID) = 0;

	// ==========================================================
	// ��Ŷ ���� �Ϸ� ��
	// [PARAM] __int64 sessionID, CPacket* packet
	// [RETURN] X 
	// ==========================================================
	virtual void OnRecv(uint32_t sessionID, CPacket* packet) = 0;

	// ==========================================================
	// ���� Ÿ�Ӿƿ� ���� ó��
	// [PARAM] __int64 sessionID
	// [RETURN] X 
	// ==========================================================
	virtual void OnTimeout(uint32_t sessionID) = 0;

	// ==========================================================
	// Error Check
	// [PARAM] int errorCode, wchar_t* msg
	// [RETURN] X 
	// ==========================================================
	virtual void OnError(int errorCode, const wchar_t* msg) = 0;

private:
	friend unsigned __stdcall AcceptThread(void* param);
	friend unsigned __stdcall WorkerThread(void* param);
	friend unsigned __stdcall TimeoutThread(void* param);

	bool AcceptThread_Serv();
	bool WorkerThread_Serv();
	bool TimeoutThread_Serv();

	// �Ϸ����� ��, �۾� ó��
	bool RecvProc(stSESSION* pSession, long cbTransferred);
	bool SendProc(stSESSION* pSession, long cbTransferred);

	// �ۼ��� ���� ��� ��, �ۼ��� �Լ� ȣ��
	bool RecvPost(stSESSION* pSession);
	bool SendPost(stSESSION* pSession);

	// ��Ŷ�� ������ �����ð��� ������ ������ ����
	// ����� Ŭ�� �α��� ���� ��Ŷ�� ������ �˾Ƽ� �����ֱ� ������ �̻��=
	bool SendPostReservedDisconnect(stSESSION* pSession);

	// ���� ���ҽ� ���� �� ����
	void ReleaseSession(stSESSION* pSession);

	// uniqueID�� index�� ������ SessionID ����
	inline uint32_t CreateSessionID(uint32_t uniqueID, int index)
	{
		return (uint32_t)((uint32_t)index | (uniqueID << SESSION_ID_BITS));
	}

	inline void ReleasePQCS(stSESSION* pSession)
	{
		PostQueuedCompletionStatus(_iocpHandle, 0, (ULONG_PTR)pSession, (LPOVERLAPPED)PQCSTYPE::RELEASE);
	}

	inline int GetSessionIndex(uint32_t sessionID)
	{
		return (int)(sessionID & SESSION_INDEX_MASK);
	}

	inline uint32_t GetSessionID(uint32_t sessionID)
	{
		return (uint32_t)(sessionID >> SESSION_ID_BITS);
	}

	// Ÿ�Ӿƿ� �ֱ� : ���� �ð� ~ ������ Ÿ�Ӿƿ� �ð� (ms ����)
	inline void SetTimeout(stSESSION* session)
	{
		InterlockedExchange(&session->_timer, timeGetTime() + _timeout);
	}

	// Ÿ�Ӿƿ� �ֱ� : ���� �ð� ~ �Ű������� ���� Ÿ�Ӿƿ� �ð� (ms ����)
	inline void SetTimeout(stSESSION* session, DWORD timeout)
	{
		InterlockedExchange(&session->_timer, timeGetTime() + timeout);
	}

private:
	// ������ ����
	SOCKET _listenSocket;								// Listen Socket
	unsigned short _serverPort;							// Server Port

	HANDLE _iocpHandle;									// IOCP Handle
	HANDLE _acceptThread;								// Accept Thread
	HANDLE _timeoutThread;								// Timeout Thread
	std::vector<HANDLE> _workerThreads;					// Worker Threads Count

	long _workerThreadCount;							// Worker Thread Count (Server)
	long _runningThreadCount;							// Running Thread Count (Server)

	long _maxAcceptCount;								// Max Accept Count

	stSESSION* _sessionArray;							// Session Array			
	LockFreeStack<int> _availableIndexStack;				// Available Session Array Index

	DWORD _serverTime;									// Server Time
	DWORD _timeout;

	enum PQCSTYPE
	{
		SENDPOST = 100,
		SENDPOSTDICONN,
		RELEASE,
		TIMEOUT,
		STOP,
	};

protected:
	bool runFlag;

protected:
	// logging
	Log* _logger;

	// ����͸��� ���� (1�� ����)
	// ������ ���Ǹ� ���� TPS�� �� ������ 1�ʴ� �߻��ϴ� �Ǽ��� ���, �������� �� ���� �հ踦 ��Ÿ��
	__int64 _acceptCount;							// Accept Session Count.
	__int64 _acceptTPS;								// Accept TPS
	__int64 _sessionCnt;							// Session Total Cnt
	__int64 _releaseCount;							// Release Session Count
	__int64 _releaseTPS;							// Release TPS
	__int64 _recvMsgTPS;							// Recv Packet TPS
	__int64 _sendMsgTPS;							// Send Packet TPS
	__int64 _recvCallTPS;							// Recv Call TPS
	__int64 _sendCallTPS;							// Send Call TPS
	__int64 _recvBytesTPS;							// Recv Bytes TPS
	__int64 _sendBytesTPS;							// Send Bytes TPS

	bool _startMonitering;
};

#endif // !__NETSERVER_CLASS__


