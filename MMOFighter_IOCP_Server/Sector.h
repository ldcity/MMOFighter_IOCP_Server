#ifndef __SECTOR__
#define __SECTOR__

#include "PCH.h"

#define SECTOR_SIZE_Y 100		// ���� �� ĭ�� ũ�� (y��)
#define SECTOR_SIZE_X 160		// ���� �� ĭ�� ũ�� (x��)

#define dfSECTOR_Y_MAX (dfRANGE_MOVE_BOTTOM / SECTOR_SIZE_Y)
#define dfSECTOR_X_MAX (dfRANGE_MOVE_RIGHT / SECTOR_SIZE_X)

class Player;


//------------------------------------------------------
// ���� �ϳ��� ��ǥ ����
//------------------------------------------------------
struct st_SECTOR_POS
{
	int x;
	int y;
};

//------------------------------------------------------
// Ư�� ��ġ �ֺ��� 9�� ���� ����
//------------------------------------------------------
struct st_SECTOR_AROUND
{
	int iCount;
	st_SECTOR_POS Around[9];
};

#endif // !__SECTOR__
