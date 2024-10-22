#ifndef  __LOCKFREESTACK_MEMORY_POOL__
#define  __LOCKFREESTACK_MEMORY_POOL__

#include <Windows.h>
#include <vector>
#include <iostream>


#define CRASH() do{ int* ptr = nullptr; *ptr = 100;} while(0)		// nullptr �����ؼ� ������ crash �߻�

#define STACKMAX 10000
#define QUEUEMAX 500
#define MAXBITS 17
#define MAX_COUNT_VALUE 47
#define PTRMASK 0x00007FFFFFFFFFFF
#define MAXIMUMADDR 0x00007FFFFFFEFFFF

template <class DATA>
class ObjectFreeListBase
{
public:
	virtual ~ObjectFreeListBase() {}
	virtual DATA* Alloc() = 0;
	virtual bool Free(DATA* ptr) = 0;
};

template <class DATA>
class ObjectLockFreeStackFreeList : ObjectFreeListBase<DATA>
{
private:
	alignas(64) uint64_t _iUseCount;								// ������� �� ����.
	alignas(64) uint64_t _iAllocCnt;
	alignas(64) uint64_t _iFreeCnt;
	alignas(64) uint64_t _iCapacity;								// ������Ʈ Ǯ ���� ��ü ����

	// ��ü �Ҵ翡 ���Ǵ� ���
	template <class DATA>
	struct st_MEMORY_BLOCK_NODE
	{
#ifdef __LOG__
		__int64 underflow;
#endif
		DATA data;
#ifdef __LOG__
		__int64 overflow;
#endif
		st_MEMORY_BLOCK_NODE<DATA>* next;
#ifdef __LOG__
		unsigned int type;
#endif
};


	// ���� ������� ��ȯ�� (�̻��) ������Ʈ ���� ����.(Top)
	alignas(64) st_MEMORY_BLOCK_NODE<DATA>* _pFreeBlockNode;

#ifdef __LOG__
	unsigned int _type;											// ������Ʈ Ǯ Ÿ��
#endif

	bool _bPlacementNew;

public:
	//////////////////////////////////////////////////////////////////////////
	// ������, �ı���.
	//
	// Parameters:	(int) �ʱ� �� ����. - 0�̸� FreeList, ���� ������ �ʱ�� �޸� Ǯ, ���߿� ��������Ʈ
	//				(bool) Alloc �� ������ / Free �� �ı��� ȣ�� ����
	// Return:
	//////////////////////////////////////////////////////////////////////////
	ObjectLockFreeStackFreeList(int iBlockNum = 0, bool bPlacementNew = false);

	virtual	~ObjectLockFreeStackFreeList();

#ifdef __LOG__
	//////////////////////////////////////////////////////////////////////////
	// �޸� Ǯ Ÿ�� ����
	//
	// Parameters: ����.
	// Return: ����
	//////////////////////////////////////////////////////////////////////////
	void PoolType(void);
#endif

	//////////////////////////////////////////////////////////////////////////
	// �� �ϳ��� �Ҵ�޴´�.  
	//
	// Parameters: ����.
	// Return: (DATA *) ����Ÿ �� ������.
	//////////////////////////////////////////////////////////////////////////
	DATA* Alloc();

	//////////////////////////////////////////////////////////////////////////
	// ������̴� ���� �����Ѵ�.
	//
	// Parameters: (DATA *) �� ������.
	// Return: (BOOL) TRUE, FALSE.
	//////////////////////////////////////////////////////////////////////////
	bool	Free(DATA* pData);


	//////////////////////////////////////////////////////////////////////////
	// ���� Ȯ�� �� �� ������ ��´�. (�޸�Ǯ ������ ��ü ����)
	//
	// Parameters: ����.
	// Return: (int) �޸� Ǯ ���� ��ü ����
	//////////////////////////////////////////////////////////////////////////
	uint64_t		GetCapacityCount(void) { return _iCapacity; }

	//////////////////////////////////////////////////////////////////////////
	// ���� ������� �� ������ ��´�.
	//
	// Parameters: ����.
	// Return: (int) ������� �� ����.
	//////////////////////////////////////////////////////////////////////////
	uint64_t		GetUseCount(void) { return _iUseCount; }

	uint64_t		GetAllocCount(void) { return _iAllocCnt; }

	uint64_t		GetFreeCount(void) { return _iFreeCnt; }

	// unique pointer
	inline st_MEMORY_BLOCK_NODE<DATA>* GetUniqu64ePointer(st_MEMORY_BLOCK_NODE<DATA>* pointer, uint64_t iCount)
	{
		return (st_MEMORY_BLOCK_NODE<DATA>*)((uint64_t)pointer | (iCount << MAX_COUNT_VALUE));
	}

	// real pointer
	inline st_MEMORY_BLOCK_NODE<DATA>* Get64Pointer(st_MEMORY_BLOCK_NODE<DATA>* uniquePointer)
	{
		return (st_MEMORY_BLOCK_NODE<DATA>*)((uint64_t)uniquePointer & PTRMASK);
	}
};

template <class DATA>
ObjectLockFreeStackFreeList<DATA>::ObjectLockFreeStackFreeList(int iBlockNum, bool bPlacementNew)
	: _pFreeBlockNode(nullptr), _bPlacementNew(bPlacementNew), _iCapacity(iBlockNum), _iUseCount(0)
{
	// x64 �𵨿��� ����� �� �ִ� �޸� �뿪�� ����ƴ��� Ȯ��
	SYSTEM_INFO si;
	GetSystemInfo(&si);

	if ((__int64)si.lpMaximumApplicationAddress != MAXIMUMADDR)
	{
		wprintf(L"X64 Maximum Address Used...\n");
		CRASH();
	}

#ifdef __LOG__
	// Ǯ Ÿ�� ����
	PoolType();
#endif

	// ���� ��������Ʈ�� �����ڿ��� �ƹ��͵� �� ��
	if (_iCapacity == 0) return;

	// �ʱ⿡ �޸� Ǯó�� ���� �޸� ����� �Ҵ� 
	//st_MEMORY_BLOCK_NODE<DATA>* memoryBlock = (st_MEMORY_BLOCK_NODE<DATA>*)malloc(sizeof(st_MEMORY_BLOCK_NODE<DATA>));
	st_MEMORY_BLOCK_NODE<DATA>* memoryBlock = (st_MEMORY_BLOCK_NODE<DATA>*)_aligned_malloc(sizeof(st_MEMORY_BLOCK_NODE<DATA>), alignof(st_MEMORY_BLOCK_NODE<DATA>));

	if (memoryBlock == nullptr)
		CRASH();

	uint64_t cnt = 0;

	_pFreeBlockNode = GetUniqu64ePointer(memoryBlock, cnt);

#ifdef __LOG__
	memoryBlock->underflow = 0xfdfdfdfdfdfdfdfd;
	memoryBlock->overflow = 0xfdfdfdfdfdfdfdfd;
	memoryBlock->type = _type;
#endif
	memoryBlock->next = nullptr;

	// placement new �� false �� ��ü ����(ó���� ��� �����Ҷ��� ������ ȣ��) - �� ���Ŀ��� ����!
	if (!_bPlacementNew)
		new(&memoryBlock->data)DATA;

	for (int i = 0; i < _iCapacity - 1; i++)
	{
		// �� ��� ����
	//st_MEMORY_BLOCK_NODE<DATA>* memoryBlock = (st_MEMORY_BLOCK_NODE<DATA>*)malloc(sizeof(st_MEMORY_BLOCK_NODE<DATA>));
		st_MEMORY_BLOCK_NODE<DATA>* memoryBlock = (st_MEMORY_BLOCK_NODE<DATA>*)_aligned_malloc(sizeof(st_MEMORY_BLOCK_NODE<DATA>), alignof(st_MEMORY_BLOCK_NODE<DATA>));
		if (memoryBlock == nullptr)
			CRASH();

		st_MEMORY_BLOCK_NODE<DATA>* newNode =
			GetUniqu64ePointer(memoryBlock, ++cnt);

#ifdef __LOG__
		memoryBlock->underflow = 0xfdfdfdfdfdfdfdfd;
		memoryBlock->overflow = 0xfdfdfdfdfdfdfdfd;
		memoryBlock->type = _type;
#endif
		memoryBlock->next = _pFreeBlockNode;
		_pFreeBlockNode = newNode;

		// placement new �� false �� ��ü ����(ó���� ��� �����Ҷ��� ������ ȣ��) - �� ���Ŀ��� ����!
		if (!_bPlacementNew)
			new(&memoryBlock->data)DATA;
	}
}

template <class DATA>
ObjectLockFreeStackFreeList<DATA>::~ObjectLockFreeStackFreeList()
{
	while (_pFreeBlockNode != nullptr)
	{
		st_MEMORY_BLOCK_NODE<DATA>* curNode = Get64Pointer(_pFreeBlockNode);

		if (!_bPlacementNew)
		{
			curNode->data.~DATA();
		}

		_pFreeBlockNode = curNode->next;
		//delete curNode;
		//free(curNode);
		_aligned_free(curNode);

	}
}

#ifdef __LOG__
//////////////////////////////////////////////////////////////////////////
// �޸� Ǯ Ÿ�� ����
//
// Parameters: ����.
// Return: ����
//////////////////////////////////////////////////////////////////////////
template <class DATA>
void ObjectLockFreeStackFreeList<DATA>::PoolType()
{
	// �޸� Ǯ �ּҰ��� type���� ����
	_type = (unsigned int)this;
}
#endif

//////////////////////////////////////////////////////////////////////////
// �� �ϳ��� �Ҵ�޴´�.  - ������带 pop
//
// Parameters: ����.
// Return: (DATA *) ����Ÿ �� ������.
//////////////////////////////////////////////////////////////////////////
template <class DATA>
DATA* ObjectLockFreeStackFreeList<DATA>::Alloc()
{
	st_MEMORY_BLOCK_NODE<DATA>* pOldTopNode = nullptr;
	st_MEMORY_BLOCK_NODE<DATA>* pOldRealNode = nullptr;
	st_MEMORY_BLOCK_NODE<DATA>* pNewTopNode = nullptr;
	uint64_t cnt;

	while (true)
	{
		pOldTopNode = _pFreeBlockNode;

		cnt = ((uint64_t)pOldTopNode >> MAX_COUNT_VALUE) + 1;

		pOldRealNode = Get64Pointer(pOldTopNode);

		// �̻�� ��(free block)�� ���� �� 
		if (pOldRealNode == nullptr)
		{
			//pOldRealNode = (st_MEMORY_BLOCK_NODE<DATA>*)malloc(sizeof(st_MEMORY_BLOCK_NODE<DATA>));
			pOldRealNode = (st_MEMORY_BLOCK_NODE<DATA>*)_aligned_malloc(sizeof(st_MEMORY_BLOCK_NODE<DATA>), alignof(st_MEMORY_BLOCK_NODE<DATA>));
			if (pOldRealNode == nullptr)
				CRASH();

#ifdef __LOG__
			pOldRealNode->underflow = 0xfdfdfdfdfdfdfdfd;
			pOldRealNode->overflow = 0xfdfdfdfdfdfdfdfd;
			pOldRealNode->type = _type;
#endif
			pOldRealNode->next = nullptr;

			//if (!_bPlacementNew)
			//	new(&pOldRealNode->data)DATA;

			// ��ü �� ���� ����
			InterlockedIncrement64((LONG64*)&_iCapacity);

			break;
		}

		pNewTopNode = pOldRealNode->next;

		if (pOldTopNode == InterlockedCompareExchangePointer((PVOID*)&_pFreeBlockNode, pNewTopNode, pOldTopNode))
		{
			break;
		}
	}

	DATA* object = &pOldRealNode->data;

	//// �� �� �����̹Ƿ� ������ ȣ��
	//if (_bPlacementNew)
	//{
	//	new(object) DATA;
	//}

	InterlockedIncrement64((LONG64*)&_iAllocCnt);

	// ������� �� ���� ����
	InterlockedIncrement64((LONG64*)&_iUseCount);

	return object;
}

//////////////////////////////////////////////////////////////////////////
// ������̴� ���� �����Ѵ�. - ������带 push
//
// Parameters: (DATA *) �� ������.
// Return: (BOOL) TRUE, FALSE.
//////////////////////////////////////////////////////////////////////////
template <class DATA>
bool ObjectLockFreeStackFreeList<DATA>::Free(DATA* pData)
//#endif
{
#ifdef __LOG__
	// ������ġ(0xfdfdfdfdfdfdfdfd) �κк��� ���� (pData�� 8byte ��)
	//st_MEMORY_BLOCK_NODE<DATA>* realTemp = (st_MEMORY_BLOCK_NODE<DATA>*)((char*)pData - sizeof(__int64));
	st_MEMORY_BLOCK_NODE<DATA>* realTemp = (st_MEMORY_BLOCK_NODE<DATA>*)((char*)pData - alignof(__int64));
#else
	st_MEMORY_BLOCK_NODE<DATA>* realTemp = (st_MEMORY_BLOCK_NODE<DATA>*)((char*)pData);
#endif

#ifdef __LOG__
	// �޸� Ǯ�� type�� �ٸ� ���
	if (realTemp->type != _type)
	{
		wprintf(L"Pool Type Failed\n");
		return false;
	}
#endif

	// ��� ���� ���� ��� ���̻� ������ ���� ���� ���
	if (_iUseCount == 0)
	{
		wprintf(L"Free Failed.\n");
		return false;
	}

#ifdef __LOG__
	// �޸� ���� ����
	if (realTemp->underflow != 0xfdfdfdfdfdfdfdfd || realTemp->overflow != 0xfdfdfdfdfdfdfdfd)
		CRASH();
#endif

	// backup�� ���� free top ���
	st_MEMORY_BLOCK_NODE<DATA>* oldTopNode = nullptr;
	st_MEMORY_BLOCK_NODE<DATA>* temp = nullptr;
	uint64_t cnt;

	while (true)
	{
		// free node top�� backup
		oldTopNode = _pFreeBlockNode;

		cnt = ((uint64_t)oldTopNode >> MAX_COUNT_VALUE) + 1;

		// �����Ϸ��� node�� ��������Ʈ�� top���� �ӽ� ����
		realTemp->next = oldTopNode;
		temp = GetUniqu64ePointer(realTemp, cnt);

		// CAS
		if (oldTopNode == InterlockedCompareExchangePointer((PVOID*)&_pFreeBlockNode, temp, oldTopNode))
		{
			break;
		}
	}

	//// �Ҹ��� ȣ�� - placement new�� true�̸� ��ü ��ȯ�� �� �Ҹ��� ȣ����Ѿ� ��
	//// CAS �۾��� �����Ͽ� free node�� push �Ϸ��ϰ��� �Ҹ��� ȣ���ؾ��� (oldNode�� �Ҹ��� ȣ��)
	//if (_bPlacementNew)
	//{
	//	realTemp->data.~DATA();
	//}
	
	InterlockedIncrement64((LONG64*)&_iFreeCnt);

	InterlockedDecrement64((LONG64*)&_iUseCount);


	return true;
}





#endif

