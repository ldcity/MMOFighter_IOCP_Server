#ifndef __LOCKFREEQUEUE__HEADER__
#define __LOCKFREEQUEUE__HEADER__

#pragma comment(lib, "winmm.lib")

#include <Windows.h>
#include <vector>

#include "LockFreeFreeList.h"

template <typename DATA>
class LockFreeQueue
{
private:
	struct Node
	{
		DATA data;
		Node* next;
	};

	alignas(64) Node* _head;
	alignas(64) Node* _tail;
	alignas(64) __int64 _size;

	ObjectLockFreeStackFreeList<Node>* LockFreeQueuePool;
	//TLSObjectPool<Node>* LockFreeQueuePool;

public:
	LockFreeQueue(int size = QUEUEMAX, int bPlacement = false) : _size(0)
	{
		LockFreeQueuePool = new ObjectLockFreeStackFreeList<Node>(size, bPlacement);

		//LockFreeQueuePool = new TLSObjectPool<Node>(size, bPlacement);


		// dummy node (dummy node�� ����Ű�� ����)
		Node* node = LockFreeQueuePool->Alloc();
		node->next = nullptr;
		_head = GetUniqu64ePointer(node, 1);
		_tail = _head;
	}

	~LockFreeQueue()
	{
		delete LockFreeQueuePool;
	}

	void Clear()
	{
	

	}

	__int64 GetSize() 
	{
		return _size; 
	}

	// unique pointer (���� �ּҰ� �ǵ��� ������� �ʵ��� ����)
	inline Node* GetUniqu64ePointer(Node* pointer, uint64_t iCount)
	{
		return (Node*)((uint64_t)pointer | (iCount << MAX_COUNT_VALUE));
	}

	// real pointer
	inline Node* Get64Pointer(Node* uniquePointer)
	{
		return (Node*)((uint64_t)uniquePointer & PTRMASK);
	}

	void Enqueue(DATA _data)
	{
		Node* realNewNode = LockFreeQueuePool->Alloc();
		if (realNewNode == nullptr)
			return;

		realNewNode->data = _data;
		realNewNode->next = nullptr;

		Node* oldTail = nullptr;
		Node* realOldTail = nullptr;
		Node* oldTailNext = nullptr;
		Node* newNode = nullptr;

		uint64_t cnt;

		while (true)
		{
			// ���� tail ���
			oldTail = _tail;

			// unique value
			cnt = ((uint64_t)oldTail >> MAX_COUNT_VALUE) + 1;

			if (cnt == 0)
				cnt = 1;

			realOldTail = Get64Pointer(oldTail);

			// ���� tail�� next ���
			oldTailNext = realOldTail->next;

			if (oldTailNext == nullptr)
			{
				// CAS 1
				if (InterlockedCompareExchangePointer((PVOID*)&realOldTail->next, realNewNode, oldTailNext) == (PVOID)oldTailNext)
				{
					newNode = GetUniqu64ePointer(realNewNode, cnt);

					// CAS 2
					// CAS 2 ���� context switching�Ǿ� �ٸ� �����尡 ��ť, ��ť ������ �ϸ鼭 oldTail
					InterlockedCompareExchangePointer((PVOID*)&_tail, newNode, oldTail);

					break;
				}

			}
			else
			{
				// CAS 2 ���� ���� _tail�� ������ ���� ������ �ȵǾ� �߻��ϴ� ����
				// -> _tail�� ������ ���� ����
				// -> ù��° CAS�� �����ϸ� enqueue�� ������ ��
				if (_tail == oldTail)
				{
					newNode = GetUniqu64ePointer(oldTailNext, cnt);
					InterlockedCompareExchangePointer((PVOID*)&_tail, newNode, oldTail);
				}
			}
		}

		InterlockedIncrement64(&_size);
	}

	bool Dequeue(DATA& _data)
	{
		// ó������ ������ ���� �� ����
		InterlockedDecrement64(&_size);

		if (_size < 0)
		{
			InterlockedIncrement64(&_size);
			return false;
		}

		uint64_t cnt;
		__int64 oldSize;

		Node* oldHead = nullptr;
		Node* raalOldHead = nullptr;
		Node* oldHeadNext = nullptr;

		Node* tmpTail = nullptr;

		while (true)
		{
			oldSize = _size;

			// head, next ���
			oldHead = _head;
			raalOldHead = Get64Pointer(oldHead);
			oldHeadNext = raalOldHead->next;



			cnt = ((uint64_t)oldHead >> MAX_COUNT_VALUE) + 1;

			if (cnt == 0)
				cnt = 1;

			// ��� head�� next�� �� ���� ���� -> �׽�Ʈ �ڵ忡�� ������ ���� ��ť, ��ť�� �ϱ� ����
			// �׷����� �߻��� �� �ִ� ����?
			// head�� ����� ��, ���ؽ�Ʈ ����Ī�� �Ͼ�� �ٸ� �����尡 �ش� ��带 ��ť, ��ť�Ͽ�
			// ���� �ּ��� ���� ���Ҵ�� ��, head�� next�� nullptr�� ������
			// �ٽ� ���ؽ�Ʈ ����Ī�Ǿ� �Ʊ��� ������� ���ƿ��� ��� head�� next�� nullptr�� ����Ǿ� �ִ� ����
			// -> ������ ť�� ����־�� ����
			if (oldHeadNext == nullptr && oldSize < 0)
			{

				wprintf(L"Queue is Empty!\n");

				InterlockedIncrement64(&_size);

				return false;
			}

			// �� �ּ�ó���� ����� ������ �߻��� ���
			// �ٸ� �����尡 �ذ����ִ� ����ۿ� �����Ƿ� ����
			if (oldHeadNext == nullptr && oldSize >= 0)
				continue;

			// tail, next ���
			Node* oldTail = _tail;
			Node* realOldTail = Get64Pointer(oldTail);

			if (realOldTail->next != nullptr)
			{
				uint64_t tailCnt = ((uint64_t)oldTail >> MAX_COUNT_VALUE) + 1;
				Node* newCurTail = GetUniqu64ePointer(realOldTail->next, tailCnt);

				// Enqueue���� _tail�� ������ ���� �����ϴ� CAS �۾��� ������ �Ŀ�
				// �ٸ� �����忡�� Dequeue�Ϸ��� ���, _tail�� ������ ���� �о����
				InterlockedCompareExchangePointer((PVOID*)&_tail, newCurTail, oldTail);
			}

			Node* newHead = GetUniqu64ePointer(oldHeadNext, cnt);

			_data = oldHeadNext->data;

			// CAS ���� (head ��ť)
			if (InterlockedCompareExchangePointer((PVOID*)&_head, newHead, oldHead) == (PVOID)oldHead)
			{
				raalOldHead->next = nullptr;
				LockFreeQueuePool->Free(raalOldHead);

				break;
			}
		}

		return true;
	}
};

#endif