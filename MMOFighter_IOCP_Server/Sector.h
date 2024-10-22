#ifndef __SECTOR__
#define __SECTOR__

#include "PCH.h"

#define SECTOR_SIZE_Y 100		// 섹터 한 칸의 크기 (y축)
#define SECTOR_SIZE_X 160		// 섹터 한 칸의 크기 (x축)

#define dfSECTOR_Y_MAX (dfRANGE_MOVE_BOTTOM / SECTOR_SIZE_Y)
#define dfSECTOR_X_MAX (dfRANGE_MOVE_RIGHT / SECTOR_SIZE_X)

class Player;


//------------------------------------------------------
// 섹터 하나의 좌표 정보
//------------------------------------------------------
struct st_SECTOR_POS
{
	int x;
	int y;
};

//------------------------------------------------------
// 특정 위치 주변의 9개 섹터 정보
//------------------------------------------------------
struct st_SECTOR_AROUND
{
	int iCount;
	st_SECTOR_POS Around[9];
};

#endif // !__SECTOR__
