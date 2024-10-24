#ifndef  __SERIALIZING_PACKET__
#define  __SERIALIZING_PACKET__

#include "PCH.h"
#define _CRT_SECURE_NO_WARNINGS

class CPacket
{
public:
	/*---------------------------------------------------------------
	Packet Enum.
	----------------------------------------------------------------*/
	enum en_PACKET
	{
		eBUFFER_DEFAULT = 256,		// 패킷의 기본 버퍼 사이즈.
		DEFAULT_HEADER_SIZE = 2,
		MMO_HEADER_SIZE = 3,
		WAN_HEADER_SIZE = 2,
		LAN_HEADER_SIZE = 2
	};

	//////////////////////////////////////////////////////////////////////////
	// 생성자, 파괴자.
	//
	// Return:
	//////////////////////////////////////////////////////////////////////////
	CPacket();
	CPacket(int iBufferSize);
	CPacket(CPacket& clSrcPacket);

	virtual	~CPacket();

	//////////////////////////////////////////////////////////////////////////
// 버퍼 늘리기.
//
// Parameters: 없음.
// Return: 없음.
//////////////////////////////////////////////////////////////////////////
	void	Resize(const char* methodName, int size);

	//////////////////////////////////////////////////////////////////////////
	// 패킷 청소.
	//
	// Parameters: 없음.
	// Return: 없음.
	//////////////////////////////////////////////////////////////////////////
	inline void	Clear(void)
	{
		_iDataSize = 0;

		readPos = writePos = _chpBuffer + DEFAULT_HEADER_SIZE;
	}


	//////////////////////////////////////////////////////////////////////////
	// 버퍼 사이즈 얻기.
	//
	// Parameters: 없음.
	// Return: (int)패킷 버퍼 사이즈 얻기.
	//////////////////////////////////////////////////////////////////////////
	inline int	GetBufferSize(void)
	{
		return _iBufferSize;
	}

	//////////////////////////////////////////////////////////////////////////
	// 버퍼 포인터 얻기.
	//
	// Parameters: 없음.
	// Return: (char *)버퍼 포인터.
	//////////////////////////////////////////////////////////////////////////
	inline char* GetBufferPtr(void)
	{
		return _chpBuffer;
	}

	//////////////////////////////////////////////////////////////////////////
	// 버퍼 Pos 이동. (음수이동은 안됨)
	// GetBufferPtr 함수를 이용하여 외부에서 강제로 버퍼 내용을 수정할 경우 사용. 
	//
	// Parameters: (int) 이동 사이즈.
	// Return: (int) 이동된 사이즈.
	//////////////////////////////////////////////////////////////////////////
	inline int		MoveWritePos(int iSize)
	{
		if (iSize < 0)
			return -1;
		if (_iBufferSize - _iDataSize - DEFAULT_HEADER_SIZE < iSize)
			return 0;

		writePos += iSize;
		_iDataSize += iSize;

		return iSize;
	}

	inline int		MoveReadPos(int iSize)
	{
		if (iSize < 0)
			return -1;
		if (_iDataSize < iSize)
			return 0;

		readPos += iSize;
		_iDataSize -= iSize;

		if (readPos == writePos) Clear();

		return iSize;
	}

	/* ============================================================================= */
	// 연산자 오버로딩
	/* ============================================================================= */
	CPacket& operator = (CPacket& clSrcPacket);

	//////////////////////////////////////////////////////////////////////////
	// 넣기.	각 변수 타입마다 모두 만듬.
	//////////////////////////////////////////////////////////////////////////
	inline CPacket& operator << (unsigned char byValue)
	{
		// 남은 사이즈보다 넣을 변수 크기가 더 크면 바로 return
		if (_iBufferSize - DEFAULT_HEADER_SIZE - _iDataSize < sizeof(unsigned char))
			Resize("operator << (unsigned char) ", sizeof(unsigned char));

		memcpy_s(writePos, sizeof(unsigned char), &byValue, sizeof(unsigned char));
		MoveWritePos(sizeof(unsigned char));

		return *this;
	}

	inline CPacket& operator << (char chValue)
	{
		// 남은 사이즈보다 넣을 변수 크기가 더 크면 바로 return
		if (_iBufferSize - DEFAULT_HEADER_SIZE - _iDataSize < sizeof(char))
			Resize("operator << (char) ", sizeof(char));

		memcpy_s(writePos, sizeof(char), &chValue, sizeof(char));
		MoveWritePos(sizeof(char));

		return *this;
	}

	inline CPacket& operator << (short shValue)
	{
		// 남은 사이즈보다 넣을 변수 크기가 더 크면 바로 return
		if (_iBufferSize - DEFAULT_HEADER_SIZE - _iDataSize < sizeof(short))
			Resize("operator << (short) ", sizeof(short));

		memcpy_s(writePos, sizeof(short), &shValue, sizeof(short));
		MoveWritePos(sizeof(short));

		return *this;
	}

	inline CPacket& operator << (unsigned short wValue)
	{
		// 남은 사이즈보다 넣을 변수 크기가 더 크면 바로 return
		if (_iBufferSize - DEFAULT_HEADER_SIZE - _iDataSize < sizeof(unsigned short))
			Resize("operator << (short) ", sizeof(unsigned short));

		memcpy_s(writePos, sizeof(unsigned short), &wValue, sizeof(unsigned short));
		MoveWritePos(sizeof(unsigned short));

		return *this;
	}

	inline CPacket& operator << (int iValue)
	{
		// 남은 사이즈보다 넣을 변수 크기가 더 크면 바로 return
		if (_iBufferSize - DEFAULT_HEADER_SIZE - _iDataSize < sizeof(int))
			Resize("operator << (int) ", sizeof(int));

		memcpy_s(writePos, sizeof(int), &iValue, sizeof(int));
		MoveWritePos(sizeof(int));

		return *this;
	}

	inline CPacket& operator << (unsigned long lValue)
	{
		// 남은 사이즈보다 넣을 변수 크기가 더 크면 바로 return
		if (_iBufferSize - DEFAULT_HEADER_SIZE - _iDataSize < sizeof(unsigned long))
			Resize("operator << (long) ", sizeof(long));

		memcpy_s(writePos, sizeof(unsigned long), &lValue, sizeof(unsigned long));
		MoveWritePos(sizeof(unsigned long));

		return *this;
	}

	inline CPacket& operator << (float fValue)
	{
		// 남은 사이즈보다 넣을 변수 크기가 더 크면 바로 return
		if (_iBufferSize - DEFAULT_HEADER_SIZE - _iDataSize < sizeof(float))
			Resize("operator << (float) ", sizeof(float));

		memcpy_s(writePos, sizeof(float), &fValue, sizeof(float));
		MoveWritePos(sizeof(float));

		return *this;
	}

	inline CPacket& operator << (__int64 iValue)
	{
		// 남은 사이즈보다 넣을 변수 크기가 더 크면 바로 return
		if (_iBufferSize - DEFAULT_HEADER_SIZE - _iDataSize < sizeof(__int64))
			Resize("operator << (__int64) ", sizeof(__int64));

		memcpy_s(writePos, sizeof(__int64), &iValue, sizeof(__int64));
		MoveWritePos(sizeof(__int64));

		return *this;
	}

	inline CPacket& operator << (double dValue)
	{
		// 남은 사이즈보다 넣을 변수 크기가 더 크면 바로 return
		if (_iBufferSize - DEFAULT_HEADER_SIZE - _iDataSize < sizeof(double))
			Resize("operator << (double) ", sizeof(double));

		memcpy_s(writePos, sizeof(double), &dValue, sizeof(double));
		MoveWritePos(sizeof(double));

		return *this;
	}

	//////////////////////////////////////////////////////////////////////////
	// 빼기.	각 변수 타입마다 모두 만듬.
	//////////////////////////////////////////////////////////////////////////
	inline CPacket& operator >> (unsigned char& byValue)
	{
		if (_iDataSize < sizeof(unsigned char))
			throw SerializingBufferException("Failed to read float", "operator >> (unsigned char)", __LINE__, this->GetBufferPtr());


		memcpy_s(&byValue, sizeof(unsigned char), readPos, sizeof(unsigned char));

		// 남은 데이터 앞으로 이동
		MoveReadPos(sizeof(unsigned char));

		return *this;
	}

	inline CPacket& operator >> (char& chValue)
	{
		if (_iDataSize < sizeof(char))
			throw SerializingBufferException("Failed to read float", "operator >> (char)", __LINE__, this->GetBufferPtr());


		memcpy_s(&chValue, sizeof(char), readPos, sizeof(char));

		// 남은 데이터 앞으로 이동
		MoveReadPos(sizeof(char));

		return *this;
	}

	inline CPacket& operator >> (short& shValue)
	{
		if (_iDataSize < sizeof(short))
			throw SerializingBufferException("Failed to read float", "operator >> (short)", __LINE__, this->GetBufferPtr());

		memcpy_s(&shValue, sizeof(short), readPos, sizeof(short));

		// 남은 데이터 앞으로 이동
		MoveReadPos(sizeof(short));

		return *this;
	}

	inline CPacket& operator >> (unsigned short& wValue)
	{
		if (_iDataSize < sizeof(unsigned short))
			throw SerializingBufferException("Failed to read float", "operator >> (unsigned short)", __LINE__, this->GetBufferPtr());


		memcpy_s(&wValue, sizeof(unsigned short), readPos, sizeof(unsigned short));

		// 남은 데이터 앞으로 이동
		MoveReadPos(sizeof(unsigned short));

		return *this;
	}

	inline CPacket& operator >> (int& iValue)
	{
		if (_iDataSize < sizeof(int))
			throw SerializingBufferException("Failed to read float", "operator >> (int)", __LINE__, this->GetBufferPtr());

		memcpy_s(&iValue, sizeof(int), readPos, sizeof(int));

		// 남은 데이터 앞으로 이동
		MoveReadPos(sizeof(int));

		return *this;
	}

	inline CPacket& operator >> (unsigned long& dwValue)
	{
		if (_iDataSize < sizeof(unsigned long))
			throw SerializingBufferException("Failed to read float", "operator >> (unsigned long)", __LINE__, this->GetBufferPtr());


		memcpy_s(&dwValue, sizeof(unsigned long), readPos, sizeof(unsigned long));

		// 남은 데이터 앞으로 이동
		MoveReadPos(sizeof(unsigned long));

		return *this;
	}

	inline CPacket& operator >> (float& fValue)
	{
		if (_iDataSize < sizeof(float))
			throw SerializingBufferException("Failed to read float", "operator >> (float)", __LINE__, this->GetBufferPtr());

		memcpy_s(&fValue, sizeof(float), readPos, sizeof(float));

		// 남은 데이터 앞으로 이동
		MoveReadPos(sizeof(float));

		return *this;
	}

	inline CPacket& operator >> (__int64& iValue)
	{
		if (_iDataSize < sizeof(__int64))
			throw SerializingBufferException("Failed to read float", "operator >> (__int64)", __LINE__, this->GetBufferPtr());

		memcpy_s(&iValue, sizeof(__int64), readPos, sizeof(__int64));

		// 남은 데이터 앞으로 이동
		MoveReadPos(sizeof(__int64));

		return *this;
	}

	inline CPacket& operator >> (double& dValue)
	{
		if (_iDataSize < sizeof(double))
			throw SerializingBufferException("Failed to read float", "operator >> (double)", __LINE__, this->GetBufferPtr());

		memcpy_s(&dValue, sizeof(double), readPos, sizeof(double));

		// 남은 데이터 앞으로 이동
		MoveReadPos(sizeof(double));

		return *this;
	}


	//////////////////////////////////////////////////////////////////////////
	// 데이타 peek
	//
	// Parameters: (char *)Dest 포인터. (int)Size.
	// Return: (int)복사한 사이즈.
	//////////////////////////////////////////////////////////////////////////
	inline int		Peek(char* chpDest, int iSize)
	{
		if (_iDataSize < iSize)
			throw SerializingBufferException("Failed to read float", "GetData", __LINE__, this->GetBufferPtr());

		memcpy_s(chpDest, iSize, _chpBuffer, iSize);

		return iSize;
	}

	inline int		LanHeaderPeek(char* chpDest, int iSize)
	{
		if (_iDataSize < iSize)
			throw SerializingBufferException("Failed to read float", "GetData", __LINE__, this->GetBufferPtr());

		memcpy_s(chpDest, iSize, LanHeaderPtr, iSize);

		return iSize;
	}

	inline int		LanPeek(char* chpDest, int iSize)
	{
		if (_iDataSize < iSize)
			throw SerializingBufferException("Failed to read float", "GetData", __LINE__, this->GetBufferPtr());

		memcpy_s(chpDest, iSize, readPos, iSize);

		return iSize;
	}

	//////////////////////////////////////////////////////////////////////////
	// 데이타 얻기.
	//
	// Parameters: (char *)Dest 포인터. (int)Size.
	// Return: (int)복사한 사이즈.
	//////////////////////////////////////////////////////////////////////////
	inline int		GetData(char* chpDest, int iSize)
	{
		if (_iDataSize < iSize)
			throw SerializingBufferException("Failed to read float", "GetData", __LINE__, this->GetBufferPtr());

		memcpy_s(chpDest, iSize, readPos, iSize);
		MoveReadPos(iSize);

		return iSize;
	}

	//////////////////////////////////////////////////////////////////////////
	// 데이타 삽입.
	//
	// Parameters: (char *)Src 포인터. (int)SrcSize.
	// Return: (int)복사한 사이즈.
	//////////////////////////////////////////////////////////////////////////
	inline int		PutData(char* chpSrc, int iSrcSize)
	{
		if (_iBufferSize - DEFAULT_HEADER_SIZE - _iDataSize < iSrcSize)
			Resize("PutData", iSrcSize);

		memcpy_s(writePos, _iBufferSize - DEFAULT_HEADER_SIZE - _iDataSize, chpSrc, iSrcSize);
		MoveWritePos(iSrcSize);

		return iSrcSize;
	}

	// 버퍼의 Front 포인터 얻음.
	inline char* GetReadBufferPtr(void)
	{
		return readPos;
	}

	// 버퍼의 RearPos 포인터 얻음.
	inline char* GetWriteBufferPtr(void)
	{
		return writePos;
	}

	// Net 헤더 포인터
	inline char* GetNetBufferPtr(void)
	{
		return _chpBuffer;
	}

	// Lan 헤더 포인터
	inline char* GetLanBufferPtr(void)
	{
		return LanHeaderPtr;
	}

	// 실제 페이로드 데이터 크기
	inline int GetDataSize()
	{
		return _iDataSize;
	}

	// 헤더 포함 페이로드 데이터 크기 (Net)
	inline int GetNetDataSize()
	{
		return _iDataSize + WAN_HEADER_SIZE;
	}

	// 헤더 포함 페이로드 데이터 크기 (Lan)
	inline int GetLanDataSize()
	{
		return _iDataSize + LAN_HEADER_SIZE;
	}

	// default 헤더 사이즈
	inline int GetDefaultHeaderSize()
	{
		return WAN_HEADER_SIZE;
	}

	// 헤더 셋팅 (Lan)
	inline void SetLanHeader()
	{
		LanHeader lanHeader;
		lanHeader.len = GetDataSize();

		memcpy_s(LanHeaderPtr, sizeof(LanHeader), &lanHeader, sizeof(LanHeader));
	}

	// 헤더 셋팅 (Wan)
	inline void SetWanHeader()
	{
		WanHeader wanHeader;
		wanHeader.len = GetDataSize() - 1;
		wanHeader.code = _code;

		// 패킷 앞 단에 네트워크 헤더 copy
		memcpy_s(_chpBuffer, sizeof(WanHeader), &wanHeader, sizeof(WanHeader));
	}

	// 프리리스트에서 할당
	inline static CPacket* Alloc()
	{
		CPacket* packet = PacketPool.Alloc();

		packet->addRefCnt();

		packet->Clear();

		return packet;
	}

	// 프리리스트에 반납
	inline static void Free(CPacket* packet)
	{
		// 패킷을 참조하는 곳이 없다면 그때서야 풀로 반환
		if (0 == packet->subRefCnt())
		{
			packet->Clear();
			PacketPool.Free(packet);
		}
	}

	inline static void SetCode(unsigned char code)
	{
		_code = code;
	}

	inline static unsigned char GetCode()
	{
		return _code;
	}

	inline static void SetKey(unsigned char key)
	{
		_key = key;
	}

	inline void addRefCnt()
	{
		InterlockedIncrement((LONG*)&ref_cnt);
	}

	inline LONG subRefCnt()
	{
		return InterlockedDecrement((LONG*)&ref_cnt);
	}

	inline static __int64 GetPoolCapacity()
	{
		return PacketPool.GetCapacity();
	}

	inline static __int64 GetPoolAllocCnt()
	{
		return PacketPool.GetObjectAllocCount();
	}

	inline static __int64 GetPoolFreeCnt()
	{
		return PacketPool.GetObjectFreeCount();
	}

	inline static __int64 GetPoolUseCnt()
	{
		return PacketPool.GetObjectUseCount();
	}

	// Packet Memory Pool
	inline static TLSObjectPool<CPacket> PacketPool = TLSObjectPool<CPacket>(2000);

private:

	// 참조 카운트
	int alignas(64) ref_cnt;

	// 직렬화 버퍼 크기
	int	_iBufferSize;

	// 현재 버퍼에 사용중인 사이즈 (payload)
	int	_iDataSize;

	// 직렬화 버퍼 할당 첫 포인터 (netPtr)
	char* _chpBuffer;

	// lan header ptr
	char* LanHeaderPtr;

	// 읽기 위치
	char* readPos;

	// 쓰기 위치
	char* writePos;

	inline static unsigned char _code;

	inline static unsigned char _key;
};

#endif