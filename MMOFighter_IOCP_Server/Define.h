#ifndef __SERVER_DEFINE__
#define __SERVER_DEFINE__

// 1ȸ �ۼ��� ��, WSABUF �ִ� ����
#define MAXWSABUF 200

// ioRefCount���� ReleaseFlag Ȯ�ο� ��Ʈ����ũ
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
	unsigned char code;			// ��Ŷ�ڵ� 0x89 ����
	unsigned char len;			// ��Ŷ ������
};
#pragma pack()

#endif