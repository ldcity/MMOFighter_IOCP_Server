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


		// dummy node (dummy node를 가리키며 시작)
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

	// unique pointer (같은 주소가 되도록 재사용되지 않도록 방지)
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
			// 기존 tail 백업
			oldTail = _tail;

			// unique value
			cnt = ((uint64_t)oldTail >> MAX_COUNT_VALUE) + 1;

			if (cnt == 0)
				cnt = 1;

			realOldTail = Get64Pointer(oldTail);

			// 기존 tail의 next 백업
			oldTailNext = realOldTail->next;

			if (oldTailNext == nullptr)
			{
				// CAS 1
				if (InterlockedCompareExchangePointer((PVOID*)&realOldTail->next, realNewNode, oldTailNext) == (PVOID)oldTailNext)
				{
					newNode = GetUniqu64ePointer(realNewNode, cnt);

					// CAS 2
					// CAS 2 전에 context switching되어 다른 스레드가 인큐, 디큐 과정을 하면서 oldTail
					InterlockedCompareExchangePointer((PVOID*)&_tail, newNode, oldTail);

					break;
				}

			}
			else
			{
				// CAS 2 실패 이후 _tail이 마지막 노드로 셋팅이 안되어 발생하는 문제
				// -> _tail을 마지막 노드로 셋팅
				// -> 첫번째 CAS가 성공하면 enqueue된 것으로 봄
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
		// 처음부터 사이즈 감소 후 진행
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

			// head, next 백업
			oldHead = _head;
			raalOldHead = Get64Pointer(oldHead);
			oldHeadNext = raalOldHead->next;



			cnt = ((uint64_t)oldHead >> MAX_COUNT_VALUE) + 1;

			if (cnt == 0)
				cnt = 1;

			// 백업 head의 next가 될 일이 없음 -> 테스트 코드에서 개수에 맞춰 인큐, 디큐를 하기 때문
			// 그럼에도 발생할 수 있는 문제?
			// head를 백업한 후, 컨텍스트 스위칭이 일어나서 다른 스레드가 해당 노드를 디큐, 인큐하여
			// 같은 주소의 노드로 재할당될 때, head의 next가 nullptr로 설정됨
			// 다시 컨텍스트 스위칭되어 아까의 스레드로 돌아오면 백업 head의 next도 nullptr로 변경되어 있는 상태
			// -> 실제로 큐가 비어있어야 진행
			if (oldHeadNext == nullptr && oldSize < 0)
			{

				wprintf(L"Queue is Empty!\n");

				InterlockedIncrement64(&_size);

				return false;
			}

			// 위 주석처리에 써놓은 문제가 발생할 경우
			// 다른 스레드가 해결해주는 방법밖에 없으므로 덤김
			if (oldHeadNext == nullptr && oldSize >= 0)
				continue;

			// tail, next 백업
			Node* oldTail = _tail;
			Node* realOldTail = Get64Pointer(oldTail);

			if (realOldTail->next != nullptr)
			{
				uint64_t tailCnt = ((uint64_t)oldTail >> MAX_COUNT_VALUE) + 1;
				Node* newCurTail = GetUniqu64ePointer(realOldTail->next, tailCnt);

				// Enqueue에서 _tail이 마지막 노드로 셋팅하는 CAS 작업이 실패한 후에
				// 다른 스레드에서 Dequeue하려는 경우, _tail을 마지막 노드로 밀어버림
				InterlockedCompareExchangePointer((PVOID*)&_tail, newCurTail, oldTail);
			}

			Node* newHead = GetUniqu64ePointer(oldHeadNext, cnt);

			_data = oldHeadNext->data;

			// CAS 성공 (head 디큐)
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