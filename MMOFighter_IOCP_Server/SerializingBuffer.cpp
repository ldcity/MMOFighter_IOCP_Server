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
		// ���� ������ clear
		Clear();

		int copyBufferSize;

		// source ���� ũ�Ⱑ dest ���� ũ�⺸�� Ŭ ���, ������ ũ��� dest ���� ũ��
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
	// ���� ����ȭ ���ۿ� �����ִ� ������ �ӽ� ���ۿ� ����
	char* temp = new char[_iDataSize];

	memcpy_s(temp, _iDataSize, _chpBuffer, _iDataSize);

	// ���� ����ȭ ���� delete
	delete[] _chpBuffer;

	int oldSize = _iBufferSize;

	// default ����ȭ ���� ũ��(�����ִ� ������ ũ�⿡�� �ʿ���ϴ� ������ ũ�⸸ŭ ���ϰ� 2��� �ø�
	_iBufferSize = (oldSize + size) * 2;

	// ���ο� ����ȭ ���� �Ҵ� & �ӽ� ���ۿ� �ִ� ������ ����
	_chpBuffer = new char[_iBufferSize];
	memcpy_s(_chpBuffer, _iBufferSize, temp, _iDataSize);

	LanHeaderPtr = _chpBuffer + WAN_HEADER_SIZE - LAN_HEADER_SIZE;
	readPos = writePos = _chpBuffer + DEFAULT_HEADER_SIZE;

	// �ӽ� ���� delete
	delete[] temp;
}


CPacket& CPacket::operator = (CPacket& clSrcPacket)
{
	if (this == &clSrcPacket) return *this;

	// ���� ������ clear
	Clear();

	int copyBufferSize;

	// source ���� ũ�Ⱑ dest ���� ũ�⺸�� Ŭ ���, ������ ũ��� dest ���� ũ��
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
