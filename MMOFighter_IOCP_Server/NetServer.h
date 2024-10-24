#ifndef __NETSERVER_CLASS__
#define __NETSERVER_CLASS__

#include "PCH.h"

// 서버간 통신 모듈 클래스
class NetServer
{
public:
	NetServer();

	~NetServer();

	// 세션 연결 종료
	bool DisconnectSession(uint32_t sessionID);

	// 패킷 전송
	bool SendPacket(uint32_t sessionID, CPacket* packet);

	// 패킷 전송 - 전송 후 끊기
	bool SendPacketAndDisconnect(uint32_t sessionID, CPacket* packet, DWORD timeout = 0);

	// Server Start
	bool Start(const wchar_t* IP, unsigned short PORT, int createWorkerThreadCnt, int runningWorkerThreadCnt, bool nagelOff, bool zeroCopyOff, int maxAcceptCnt, unsigned char packet_code, unsigned char packet_key, DWORD timeout);

	// Server Stop
	void Stop();

protected:
	// ==========================================================
	// White IP Check 
	// [PARAM] const wchar_t* IP, unsigned short PORT
	// [RETURN] TRUE : 접속 허용 / FALSE : 접속 거부
	// ==========================================================
	virtual bool OnConnectionRequest(const wchar_t* IP, unsigned short PORT) = 0;

	// ==========================================================
	// 접속처리 완료 후 호출 
	// [PARAM] __int64 sessionID
	// [RETURN] X
	// ==========================================================
	virtual void OnClientJoin(uint32_t sessionID) = 0;

	// ==========================================================
	// 접속해제 후 호출, Player 관련 리소스 해제
	// [PARAM] __int64 sessionID
	// [RETURN] X 
	// ==========================================================
	virtual void OnClientLeave(uint32_t sessionID) = 0;

	// ==========================================================
	// 패킷 수신 완료 후
	// [PARAM] __int64 sessionID, CPacket* packet
	// [RETURN] X 
	// ==========================================================
	virtual void OnRecv(uint32_t sessionID, CPacket* packet) = 0;

	// ==========================================================
	// 세션 타임아웃 관련 처리
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

	// 완료통지 후, 작업 처리
	bool RecvProc(stSESSION* pSession, long cbTransferred);
	bool SendProc(stSESSION* pSession, long cbTransferred);

	// 송수신 버퍼 등록 후, 송수신 함수 호출
	bool RecvPost(stSESSION* pSession);
	bool SendPost(stSESSION* pSession);

	// 패킷을 보내고 일정시간이 지나면 세션을 끊음
	// 현재는 클라가 로그인 응답 패킷을 받으면 알아서 끊어주기 때문에 미사용=
	bool SendPostReservedDisconnect(stSESSION* pSession);

	// 세션 리소스 정리 및 해제
	void ReleaseSession(stSESSION* pSession);

	// uniqueID와 index로 고유한 SessionID 생성
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

	// 타임아웃 주기 : 현재 시간 ~ 서버의 타임아웃 시간 (ms 단위)
	inline void SetTimeout(stSESSION* session)
	{
		InterlockedExchange(&session->_timer, timeGetTime() + _timeout);
	}

	// 타임아웃 주기 : 현재 시간 ~ 매개변수로 받은 타임아웃 시간 (ms 단위)
	inline void SetTimeout(stSESSION* session, DWORD timeout)
	{
		InterlockedExchange(&session->_timer, timeGetTime() + timeout);
	}

private:
	// 서버용 변수
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

	// 모니터링용 변수 (1초 기준)
	// 이해의 편의를 위해 TPS가 들어간 변수는 1초당 발생하는 건수를 계산, 나머지는 총 누적 합계를 나타냄
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


