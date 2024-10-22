#ifndef __PLAYER__
#define __PLAYER__

#include "PCH.h"

struct Player
{
	ULONG32 sessionID;			// 세션 ID

	unsigned char dwAction;		// 현재 동작
	unsigned char byDirection;	// 바라보는 방향

	short sectorX;				// 플레이어 x좌표
	short sectorY;				// 플레이어 y좌표

	st_SECTOR_POS CurSector;	// 현재 섹터 좌표
	st_SECTOR_POS OldSector;	// 이전 섹터 좌표

	char cHP;					// 체력
};

#endif