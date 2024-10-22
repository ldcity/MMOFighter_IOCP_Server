#ifndef __SERVER_DEFINE__
#define __SERVER_DEFINE__

// 1회 송수신 시, WSABUF 최대 제한
#define MAXWSABUF 200

// ioRefCount에서 ReleaseFlag 확인용 비트마스크
#define RELEASEMASKING 0x80000000

#define IDMAXLEN			20
#define NICKNAMEMAXLEN		20
#define MSGMAXLEN			64

#define GAMESERVERIP		16
#define CHATSERVERIIP		16

#define MAXPACKETLEN		32

#pragma pack(1)
// LAN Header
struct LanHeader
{
	// Payload Len
	short len;
};

// WAN Header
struct WanHeader
{
	unsigned char code;			// 패킷코드 0x89 고정
	unsigned char len;			// 패킷 사이즈
};
#pragma pack()

#endif