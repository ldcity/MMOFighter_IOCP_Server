#ifndef __PLAYER__
#define __PLAYER__

#include "PCH.h"

struct Player
{
	ULONG32 sessionID;			// ���� ID

	unsigned char dwAction;		// ���� ����
	unsigned char byDirection;	// �ٶ󺸴� ����

	short sectorX;				// �÷��̾� x��ǥ
	short sectorY;				// �÷��̾� y��ǥ

	st_SECTOR_POS CurSector;	// ���� ���� ��ǥ
	st_SECTOR_POS OldSector;	// ���� ���� ��ǥ

	char cHP;					// ü��
};

#endif