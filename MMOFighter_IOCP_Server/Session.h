#ifndef __SESSION__
#define __SESSION__

#include "RingBuffer.h"
#include "LockFreeQueue.h"

const char SESSION_ID_BITS = 17;
const __int32 SESSION_INDEX_MASK = 0x1FFFF;

// Session Struct
struct stSESSION
{
	uint32_t _sessionID;											// Session ID
	SOCKET _socketClient;										// Client Socket

	DWORD _timeout;												// Last Recv Time (타임아웃 주기)
	DWORD _timer;												// Timeout Timer (타임아웃 시간 설정)

	OVERLAPPED _stRecvOverlapped;								// Recv Overlapped I/O Struct
	OVERLAPPED _stSendOverlapped;								// Send Overlapped I/O Struct

	RingBuffer _recvRingBuffer;									// Recv RingBuffer
	CPacket* _sendPackets[MAXWSABUF] = { nullptr };			// Send Packets 배열
	LockFreeQueue<CPacket*> _sendQ;								// Send LockFreeQueue

	alignas(64) int _sendPacketCount;							// WSABUF Count
	alignas(32) DWORD _ioRefCount;								// I/O Count & Session Ref Count
	alignas(64) bool _sendFlag;									// Sending Message Check
	alignas(64) bool _isDisconnected;							// Session Disconnected
	alignas(64) bool _sendDisconnFlag;

	stSESSION()
	{
		_sessionID = -1;
		_socketClient = INVALID_SOCKET;

		_timeout = 0;
		_timer = 0;

		ZeroMemory(&_stRecvOverlapped, sizeof(OVERLAPPED));
		ZeroMemory(&_stSendOverlapped, sizeof(OVERLAPPED));
		_recvRingBuffer.ClearBuffer();

		_sendPacketCount = 0;
		_ioRefCount = 0;			// accept 이후 바로 recv 걸어버리기 때문에 항상 default가 1
		_sendFlag = false;
		_isDisconnected = false;
		_sendDisconnFlag = false;
	}

	~stSESSION()
	{
	}
};


struct stLanSESSION
{
	uint32_t sessionID;											// Session ID
	SOCKET _socketClient;										// Client Socket
	uint32_t IP_num;											// Server IP

	wchar_t IP_str[20];											// String IP

	unsigned short PORT;										// Server PORT

	DWORD LastRecvTime;											// Last Recv Time
	DWORD Timer;												// Timeout Timer

	OVERLAPPED _stRecvOverlapped;								// Recv Overlapped I/O Struct
	OVERLAPPED _stSendOverlapped;								// Send Overlapped I/O Struct

	RingBuffer recvRingBuffer;									// Recv RingBuffer
	LockFreeQueue<CPacket*> sendQ;								// Send LockFreeQueue

	CPacket* SendPackets[MAXWSABUF] = { nullptr };			// Send Packets 배열

	alignas(64) int sendPacketCount;							// WSABUF Count
	alignas(64) __int64 ioRefCount;								// I/O Count & Session Ref Count
	alignas(64) bool sendFlag;									// Sending Message Check
	alignas(8) bool isDisconnected;								// Session Disconnected
	alignas(8) bool isUsed;										// Session Used

	stLanSESSION()
	{
		sessionID = -1;
		_socketClient = INVALID_SOCKET;
		ZeroMemory(IP_str, sizeof(IP_str));
		IP_num = 0;
		PORT = 0;

		LastRecvTime = 0;
		Timer = 0;

		ZeroMemory(&_stRecvOverlapped, sizeof(OVERLAPPED));
		ZeroMemory(&_stSendOverlapped, sizeof(OVERLAPPED));
		recvRingBuffer.ClearBuffer();

		sendPacketCount = 0;
		ioRefCount = 0;			// accept 이후 바로 recv 걸어버리기 때문에 항상 default가 1
		sendFlag = false;
		isDisconnected = false;
		isUsed = false;
	}

	~stLanSESSION()
	{
	}
};
#endif // !__SESSION__
