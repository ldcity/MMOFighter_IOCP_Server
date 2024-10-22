#include "PCH.h"

#include "SerializingBuffer.h"

CPacket::CPacket() : _iDataSize(0), _iBufferSize(eBUFFER_DEFAULT), _chpBuffer(nullptr), ref_cnt(0)
{
	_chpBuffer = new char[_iBufferSize + 1];

	LanHeaderPtr = _chpBuffer + WAN_HEADER_SIZE - LAN_HEADER_SIZE;
	readPos = writePos = _chpBuffer + DEFAULT_HEADER_SIZE;
}

CPacket::CPacket(int iBufferSize) : _iDataSize(0), _iBufferSize(iBufferSize), _chpBuffer(nullptr), ref_cnt(0)
{
	_chpBuffer = new char[_iBufferSize + 1];

	LanHeaderPtr = _chpBuffer + WAN_HEADER_SIZE - LAN_HEADER_SIZE;
	readPos = writePos = _chpBuffer + DEFAULT_HEADER_SIZE;
}

CPacket::CPacket(CPacket& clSrcPacket) : _iDataSize(0), _iBufferSize(eBUFFER_DEFAULT), _chpBuffer(nullptr)
{
	if (this != &clSrcPacket)
	{
		// 기존 데이터 clear
		Clear();

		int copyBufferSize;

		// source 버퍼 크기가 dest 버퍼 크기보다 클 경우, 복사할 크기는 dest 버퍼 크기
		if (_iBufferSize < clSrcPacket._iBufferSize)
			copyBufferSize = _iBufferSize;
		else
			copyBufferSize = clSrcPacket._iBufferSize;

		memcpy_s(_chpBuffer, _iBufferSize, clSrcPacket.GetBufferPtr(), copyBufferSize);

		_iDataSize = clSrcPacket._iDataSize;
		_iBufferSize = clSrcPacket._iBufferSize;

		LanHeaderPtr = clSrcPacket.LanHeaderPtr;

		readPos = clSrcPacket.readPos;
		writePos = clSrcPacket.writePos;
	}
}

CPacket::~CPacket()
{
	Clear();
	if (_chpBuffer != nullptr)
	{
		delete[] _chpBuffer;
		_chpBuffer = nullptr;
	}
}

void CPacket::Resize(const char* methodName, int size)
{
	// 기존 직렬화 버퍼에 남아있던 데이터 임시 버퍼에 복사
	char* temp = new char[_iDataSize];

	memcpy_s(temp, _iDataSize, _chpBuffer, _iDataSize);

	// 기존 직렬화 버퍼 delete
	delete[] _chpBuffer;

	int oldSize = _iBufferSize;

	// default 직렬화 버퍼 크기(남아있는 데이터 크기에서 필요로하는 데이터 크기만큼 더하고 2배로 늘림
	_iBufferSize = (oldSize + size) * 2;

	// 새로운 직렬화 버퍼 할당 & 임시 버퍼에 있던 데이터 복사
	_chpBuffer = new char[_iBufferSize];
	memcpy_s(_chpBuffer, _iBufferSize, temp, _iDataSize);

	LanHeaderPtr = _chpBuffer + WAN_HEADER_SIZE - LAN_HEADER_SIZE;
	readPos = writePos = _chpBuffer + DEFAULT_HEADER_SIZE;

	// 임시 버퍼 delete
	delete[] temp;
}


CPacket& CPacket::operator = (CPacket& clSrcPacket)
{
	if (this == &clSrcPacket) return *this;

	// 기존 데이터 clear
	Clear();

	int copyBufferSize;

	// source 버퍼 크기가 dest 버퍼 크기보다 클 경우, 복사할 크기는 dest 버퍼 크기
	if (_iBufferSize < clSrcPacket._iBufferSize)
		copyBufferSize = _iBufferSize;
	else
		copyBufferSize = clSrcPacket._iBufferSize;

	memcpy_s(_chpBuffer, _iBufferSize, clSrcPacket.GetBufferPtr(), copyBufferSize);

	_iDataSize = clSrcPacket._iDataSize;
	_iBufferSize = clSrcPacket._iBufferSize;

	LanHeaderPtr = _chpBuffer + WAN_HEADER_SIZE - LAN_HEADER_SIZE;

	readPos = clSrcPacket.readPos;
	writePos = clSrcPacket.writePos;

	return *this;
}
