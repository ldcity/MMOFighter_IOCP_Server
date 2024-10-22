#include "PCH.h"

#include "MMOServer.h"

// Job Worker Thread
unsigned __stdcall JobWorkerThread(PVOID param)
{
	MMOServer* mmoServ = (MMOServer*)param;

	mmoServ->JobWorkerThread_Serv();

	return 0;
}


// Worker Thread Call
unsigned __stdcall MoniteringThread(void* param)
{
	MMOServer* mmoServ = (MMOServer*)param;

	mmoServ->MoniterThread_Serv();

	return 0;
}

MMOServer::MMOServer()
{

}

MMOServer::~MMOServer()
{
	MMOServerStop();
}

bool MMOServer::MMOServerStart()
{
	_log = new Log(L"MMOLog.txt");

	// login server ���������� Parsing�Ͽ� �о��
	TextParser loginServerInfoTxt;

	const wchar_t* txtName = L"MMOServer.txt";
	loginServerInfoTxt.LoadFile(txtName);

	loginServerInfoTxt.GetValue(L"SERVER.BIND_IP", _ip);
	loginServerInfoTxt.GetValue(L"SERVER.BIND_PORT", &_port);

	_wIP = _ip;
	int len = WideCharToMultiByte(CP_UTF8, 0, _wIP.c_str(), -1, NULL, 0, NULL, NULL);
	std::string result(len - 1, '\0');
	WideCharToMultiByte(CP_UTF8, 0, _wIP.c_str(), -1, &result[0], len, NULL, NULL);
	_IP = result;

	_performMoniter.AddInterface(_IP);

	int workerThread;
	loginServerInfoTxt.GetValue(L"SERVER.IOCP_WORKER_THREAD", &workerThread);

	int runningThread;
	loginServerInfoTxt.GetValue(L"SERVER.IOCP_ACTIVE_THREAD", &runningThread);

	int nagleOff;
	loginServerInfoTxt.GetValue(L"SERVER.NAGLE_OFF", &nagleOff);

	int zeroCopyOff;
	loginServerInfoTxt.GetValue(L"SERVER.ZEROCOPY_OFF", &zeroCopyOff);

	int sessionMAXCnt;
	loginServerInfoTxt.GetValue(L"SERVER.SESSION_MAX", &sessionMAXCnt);

	loginServerInfoTxt.GetValue(L"SERVER.USER_MAX", &_userMAXCnt);

	int packetCode;
	loginServerInfoTxt.GetValue(L"SERVER.PACKET_CODE", &packetCode);

	loginServerInfoTxt.GetValue(L"SERVICE.TIMEOUT_DISCONNECT", &_timeout);

	// Network Logic Start
	bool ret = this->Start(_ip, _port, workerThread, runningThread, nagleOff, zeroCopyOff, sessionMAXCnt, packetCode, 0, _timeout);
	if (!ret)
	{
		_log->logger(dfLOG_LEVEL_ERROR, __LINE__, L"NetServer Start Error");
		return false;
	}

	// Create Manual Event
	_runEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (_runEvent == NULL)
	{
		int eventError = WSAGetLastError();
		_log->logger(dfLOG_LEVEL_ERROR, __LINE__, L"CreateEvent() Error : %d", eventError);

		return false;
	}

	// Create Auto Event
	_moniterEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (_moniterEvent == NULL)
	{
		int eventError = WSAGetLastError();
		_log->logger(dfLOG_LEVEL_ERROR, __LINE__, L"CreateEvent() Error : %d", eventError);

		return false;
	}
	// Create Auto Event
	_jobEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (_jobEvent == NULL)
	{
		int eventError = WSAGetLastError();
		_log->logger(dfLOG_LEVEL_ERROR, __LINE__, L"CreateEvent() Error : %d", eventError);

		return false;
	}

	// Monitering Thread
	_moniteringThread = (HANDLE)_beginthreadex(NULL, 0, MoniteringThread, this, CREATE_SUSPENDED, NULL);
	if (_moniteringThread == NULL)
	{
		int threadError = GetLastError();
		_log->logger(dfLOG_LEVEL_ERROR, __LINE__, L"_beginthreadex() Error : %d", threadError);

		return false;
	}

	// Job Worker Thread
	_jobHandle = (HANDLE)_beginthreadex(NULL, 0, JobWorkerThread, this, 0, NULL);
	if (_jobHandle == NULL)
	{
		int threadError = GetLastError();
		_log->logger(dfLOG_LEVEL_ERROR, __LINE__, L"_beginthreadex() Error : %d", threadError);
		return false;
	}

	_log->logger(dfLOG_LEVEL_DEBUG, __LINE__, L"Create Job Worker Thread");

	WaitForSingleObject(_moniteringThread, INFINITE);
	WaitForSingleObject(_jobHandle, INFINITE);

	return true;
}

bool MMOServer::MMOServerStop()
{
	CloseHandle(_jobHandle);
	CloseHandle(_jobEvent);

	CloseHandle(_moniteringThread);
	CloseHandle(_moniterEvent);
	CloseHandle(_runEvent);

	this->Stop();

	return true;
}

// Monitering Thread
bool MMOServer::MoniterThread_Serv()
{
	DWORD threadID = GetCurrentThreadId();

	while (true)
	{
		// 1�ʸ��� ����͸� -> Ÿ�Ӿƿ� �ǵ� ó��
		DWORD ret = WaitForSingleObject(_moniterEvent, 1000);

		if (ret == WAIT_TIMEOUT)
		{
			__int64 jobPoolCapacity = _jobPool.GetCapacity();
			__int64 jobPoolUseCnt = _jobPool.GetObjectUseCount();
			__int64 jobPoolAllocCnt = _jobPool.GetObjectAllocCount();
			__int64 jobPoolFreeCnt = _jobPool.GetObjectFreeCount();

			__int64 packetPoolCapacity = CPacket::GetPoolCapacity();
			__int64 packetPoolUseCnt = CPacket::GetPoolUseCnt();
			__int64 packetPoolAllocCnt = CPacket::GetPoolAllocCnt();
			__int64 packetPoolFreeCnt = CPacket::GetPoolFreeCnt();

			wprintf(L"------------------------[Moniter]----------------------------\n");
			_performMoniter.PrintMonitorData();

			wprintf(L"------------------------[Network]----------------------------\n");
			wprintf(L"[Session              ] Total      : %10I64d\n", _sessionCnt);
			wprintf(L"[Accept               ] Total      : %10I64d      TPS        : %10I64d\n", _acceptCount, InterlockedExchange64(&_acceptTPS, 0));
			wprintf(L"[Release              ] Total      : %10I64d      TPS        : %10I64d\n", _releaseCount, InterlockedExchange64(&_releaseTPS, 0));
			wprintf(L"-------------------[Chatting Contents]------------------------\n");
			wprintf(L"[JobQ                 ] Main       : %10I64d\n", _jobQ.GetSize());
			wprintf(L"[Main Job             ] TPS        : %10I64d      Total      : %10I64d\n", _jobUpdateTPS, _jobUpdateCnt);
			wprintf(L"[Job Pool Use         ] Use        : %10llu\n", jobPoolUseCnt);
			wprintf(L"[Packet Pool          ] Capacity   : %10llu      Use        : %10llu      Alloc : %10llu    Free : %10llu\n",
				packetPoolCapacity, packetPoolUseCnt, packetPoolAllocCnt, packetPoolFreeCnt);
			wprintf(L"[Player               ] Total      : %10I64d      TPS        : %10I64d\n", _playerCnt, InterlockedExchange64(&_playerTPS, 0));
			wprintf(L"==============================================================\n\n");
		}
	}

	return true;
}



bool MMOServer::JobWorkerThread_Serv()
{
	DWORD threadID = GetCurrentThreadId();

	while (true)
	{
		Job* job = nullptr;

		// ������Ʈ (25fps)
		Update();

		// Job�� ���� ������ update �ݺ�
		while (_jobQ.GetSize() > 0)
		{
			if (_jobQ.Dequeue(job))
			{
				// Job Type�� ���� �б� ó��
				switch (job->type)
				{
				case JobType::NEW_CONNECT:
					CreatePlayer(job->sessionID);				// Player ����
					break;

				case JobType::DISCONNECT:
					DeletePlayer(job->sessionID);				// Player ����
					break;

				case JobType::MSG_PACKET:
					PacketProc(job->sessionID, job->packet);	// ��Ŷ ó��
					break;

				case JobType::TIMEOUT:
					DisconnectSession(job->sessionID);			// Ÿ�� �ƿ�
					break;

				default:
					DisconnectSession(job->sessionID);
					break;
				}

				// ����, ���� Job�� packet�� nullptr�̱� ������ ��ȯ�� Packet�� ����
				if (job->packet != nullptr)
					CPacket::Free(job->packet);

				// JobPool�� Job ��ü ��ȯ
				_jobPool.Free(job);

				InterlockedIncrement64(&_jobUpdateCnt);
				InterlockedIncrement64(&_jobUpdateTPS);
			}
		}
	}
}

bool MMOServer::OnConnectionRequest(const wchar_t* IP, unsigned short PORT)
{

	return true;
}


// ���� ó�� - �� �� Player �����⸦ �̸� ����������
// �α��� ���� ���Ḹ �� ���ǿ� ���� timeout�� �Ǵ��ؾ��ϱ� ������
void MMOServer::OnClientJoin(uint32_t sessionID)
{
	if (!_startFlag)
	{
		ResumeThread(_moniteringThread);
		_startFlag = true;
	}

	Job* job = _jobPool.Alloc();			// jobPool���� job �Ҵ�
	job->type = JobType::NEW_CONNECT;		// �� ���� ����
	job->sessionID = sessionID;				// Player�� �ο��� SessionID
	job->packet = nullptr;					// Player ���� �ÿ��� ��Ŷ �ʿ� ����

	_jobQ.Enqueue(job);					// JobQ�� Enqueue
	//SetEvent(_jobEvent);					// Job Worker Thread Event �߻�
}

// ���� ó��
void MMOServer::OnClientLeave(uint32_t sessionID)
{
	Job* job = _jobPool.Alloc();
	job->type = JobType::DISCONNECT;
	job->sessionID = sessionID;
	job->packet = nullptr;

	_jobQ.Enqueue(job);
	//SetEvent(_jobEvent);
}

// ��Ŷ ó��
void MMOServer::OnRecv(uint32_t sessionID, CPacket* packet)
{
	Job* job = _jobPool.Alloc();
	job->type = JobType::MSG_PACKET;
	job->sessionID = sessionID;
	job->packet = packet;

	_jobQ.Enqueue(job);
	//SetEvent(_jobEvent);
}

// Network Logic ���κ��� timeout ó���� �߻��Ǹ� timeout Handler ȣ��
void MMOServer::OnTimeout(uint32_t sessionID)
{

}

void MMOServer::OnError(int errorCode, const wchar_t* msg)
{

}

// player ���� & �ֺ� ������Ʈ ��Ŷ ����
bool MMOServer::CreatePlayer(uint32_t sessionID)
{
	Player* player = _playerPool.Alloc();

	if (player == nullptr)
	{
		_log->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Player Pool Alloc Failed!");

		CRASH();

		return false;
	}

	player->sessionID = sessionID;					// sessionID ����
	player->dwAction = dfPACKET_MOVE_STOP;
	player->byDirection = rand() % 2 ? dfPACKET_MOVE_DIR_LL : dfPACKET_MOVE_DIR_RR;
	player->sectorX = rand() % 1000;
	player->sectorY = rand() % 1000;

	player->cHP = 100;
	player->OldSector.x = player->sectorX / SECTOR_SIZE_X;
	player->OldSector.y = player->sectorY / SECTOR_SIZE_Y;
	player->CurSector.x = player->sectorX / SECTOR_SIZE_X;
	player->CurSector.y = player->sectorY / SECTOR_SIZE_Y;

	_mapPlayer.insert({ sessionID, player });		// ��ü Player�� �����ϴ� map�� insert
	_sector[player->CurSector.y][player->CurSector.x].insert(player);

	InterlockedIncrement64(&_playerCnt);
	InterlockedIncrement64(&_playerTPS);

	CPacket* myPacket = CPacket::Alloc();
	CPacket* otherPacket = CPacket::Alloc();
	CPacket* otherPacket2 = CPacket::Alloc();
	CPacket* movePacket = CPacket::Alloc();

	// ���ο� �÷��̾� ��Ŷ
	mpCreateMe(myPacket, player->sessionID, player->byDirection, player->sectorX, player->sectorY, player->cHP);
	SendPacket(sessionID, myPacket);

	// �ֺ� ������ �ٸ� �÷��̾�鿡�� ���ο� �÷��̾� ��Ŷ ����
	mpCreateOther(otherPacket, player->sessionID, player->byDirection, player->sectorX, player->sectorY, player->cHP);
	SendPacketAround(player, otherPacket);

	// �ֺ� ���� ���� ���
	st_SECTOR_AROUND sectorAround;
	GetSectorAround(player->CurSector.x, player->CurSector.y, &sectorAround);

	// �ٸ� �÷��̾���� ���� ��Ŷ���� ���� �����ϴ� �÷��̾�� ����
	for (int i = 0; i < sectorAround.iCount; i++)
	{
		auto iter = _sector[sectorAround.Around[i].y][sectorAround.Around[i].x].begin();
		for (; iter != _sector[sectorAround.Around[i].y][sectorAround.Around[i].x].end();)
		{
			Player* pExistPlayer = *iter;
			++iter;

			// �� �÷��̾� ������ ��� ���� (���� ���� ���� �ִ� �ٸ� �÷��̾���� ���� ��Ŷ�� ������ ������ ���̹Ƿ�)
			if (pExistPlayer == player)
				continue;

			// �ٸ� �÷��̾� ���� ���� ����
			mpCreateOther(otherPacket2, pExistPlayer->sessionID,
				pExistPlayer->byDirection,
				pExistPlayer->sectorX,
				pExistPlayer->sectorY, 
				pExistPlayer->cHP);
			SendPacket(sessionID, otherPacket2);

			// �ٸ� ������ ĳ���Ͱ� �̵� ���̾��ٸ� �̵� ��Ŷ ���� ����
			if (pExistPlayer->dwAction != dfPACKET_MOVE_STOP)
			{
				mpMoveStart(movePacket,
					pExistPlayer->sessionID, 
					pExistPlayer->dwAction,
					pExistPlayer->sectorX, 
					pExistPlayer->sectorY);
				SendPacket(sessionID, movePacket);
			}
		}
	}

	CPacket::Free(myPacket);
	CPacket::Free(otherPacket);
	CPacket::Free(otherPacket2);
	CPacket::Free(movePacket);

	return true;
}

// player ����
bool MMOServer::DeletePlayer(uint32_t sessionID)
{
	// player �˻�
	Player* player = FindPlayer(sessionID);

	if (player == nullptr)
	{
		_log->logger(dfLOG_LEVEL_DEBUG, __LINE__, L"DeletePlayer # Player Not Found! ID : %016llx", sessionID);
		CRASH();
		return false;
	}

	CPacket* deletePacket = CPacket::Alloc();

	// ���Ϳ��� �ش� player ��ü ����
	_sector[player->CurSector.y][player->CurSector.x].erase(player);

	// �ֺ� ���Ϳ� ���� ��Ŷ ���� (���� ���� ����)
	mpDelete(deletePacket, sessionID);
	SendPacketAround(player, deletePacket);

	// ��ü Player ���� map���� �ش� player ����
	_mapPlayer.erase(player->sessionID);								

	InterlockedDecrement64(&_playerCnt);

	CPacket::Free(deletePacket);
	_playerPool.Free(player);		// PlayerPool�� player ��ȯ

	return true;
}

bool MMOServer::PacketProc(uint32_t sessionID, CPacket* packet)
{
	unsigned char type;
	*packet >> type;

	switch (type)
	{
	case dfPACKET_CS_MOVE_START:
		NetPacketProc_MoveStart(sessionID, packet);
		break;

	case dfPACKET_CS_MOVE_STOP:
		NetPacketProc_MoveStop(sessionID, packet);
		break;

	case dfPACKET_CS_ATTACK1:
		NetPacketProc_Attack1(sessionID, packet);
		break;

	case dfPACKET_CS_ATTACK2:
		NetPacketProc_Attack2(sessionID, packet);
		break;

	case dfPACKET_CS_ATTACK3:
		NetPacketProc_Attack3(sessionID, packet);
		break;

	case dfPACKET_CS_ECHO:
		NetPacketProc_Echo(sessionID, packet);
		break;

	case dfPACKET_ON_TIMEOUT:
		DisconnectSession(sessionID);					// ���� Ÿ�Ӿƿ�
		break;

	default:
		// �߸��� ��Ŷ
		_log->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Packet Type Error > %d", type);
		DisconnectSession(sessionID);
		break;
	}

	return true;
}

bool MMOServer::NetPacketProc_MoveStart(uint32_t sessionID, CPacket* packet)
{
	unsigned char byDirection;
	short shX;
	short shY;

	// player ã��
	Player* player = FindPlayer(sessionID);
	if (player == nullptr)
	{
		_log->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Start # %016llx Player Not Found!", sessionID);
		return false;
	}

	*packet >> byDirection >> shX >> shY;

	CPacket* movePacket = CPacket::Alloc();

	// ������ ��ġ�� ���� ��Ŷ�� ��ġ���� �ʹ� ū ���̰� �� ���, ��ũ ��Ŷ�� ���� ��ǥ ����
	if (abs(player->sectorX - shX) > dfERROR_RANGE || abs(player->sectorY - shY) > dfERROR_RANGE)
	{
		_log->logger(dfLOG_LEVEL_ERROR, __LINE__, L"# SYNC > Session ID: %d / MoveDirection: %d / ServerX:%d / ServerY:%d / ClientX: %d / ClientY: %d ",
			player->sessionID, byDirection, player->sectorX, player->sectorY, shX, shY);

		CPacket* syncPacket = CPacket::Alloc();

		mpMySync(syncPacket, player->sessionID, player->sectorX, player->sectorY);

		// player�� �����ϴ� ������ �ֺ� 9�� ���� ���� �� ��ũ ��Ŷ ����
		SendPacketAround(player, syncPacket, true);

		shX = player->sectorX;
		shY = player->sectorY;

		CPacket::Free(syncPacket);
	}

	// ������ ����. ���۹�ȣ��, ���Ⱚ�� ����
	player->dwAction = byDirection;

	// �ٶ󺸴� ������ ����
	switch (byDirection)
	{
	case dfPACKET_MOVE_DIR_RR:
	case dfPACKET_MOVE_DIR_RU:
	case dfPACKET_MOVE_DIR_RD:
		player->byDirection = dfPACKET_MOVE_DIR_RR;
		break;

	case dfPACKET_MOVE_DIR_LL:
	case dfPACKET_MOVE_DIR_LU:
	case dfPACKET_MOVE_DIR_LD:
		player->byDirection = dfPACKET_MOVE_DIR_LL;
		break;
	}

	// Ŭ�󿡼� �� ��Ŷ(Ŭ���� ��ǥ ������)�� ������ �״�� ���� 
	// -> ��ǥ Ʋ������ ������ �� ����
	player->sectorX = shX;
	player->sectorY = shY;

	// ���� ������ ũ��� �̵� �Ŀ� ���� ������ ũ��
	int oldSize = _sector[player->OldSector.y][player->OldSector.x].size();
	int curSize = _sector[player->CurSector.y][player->CurSector.x].size();

	// ������ �ϸ鼭 ��ǥ�� �ణ ������ ���, ���� ������Ʈ�� ��
	if (SectorUpdateCharacter(player))
	{
		// ���Ͱ� ����� ���� Ŭ�󿡰� ���� ������ �� 
		CharacterSectorUpdatePacket(player);
	}

	// �̵� ���� ��Ŷ
	mpMoveStart(movePacket, sessionID, player->dwAction, player->sectorX, player->sectorY);

	// ���� �ֺ� ���� ��ο��� ��Ŷ ����
	SendPacketAround(player, movePacket);

	CPacket::Free(movePacket);

	return true;
}

bool MMOServer::NetPacketProc_MoveStop(uint32_t sessionID, CPacket* packet)
{
	unsigned char byDirection;
	short shX;
	short shY;

	// player ã��
	Player* player = FindPlayer(sessionID);
	if (player == nullptr)
	{
		_log->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Stop # %016llx Player Not Found!", sessionID);
		CRASH();
	}

	*packet >> byDirection >> shX >> shY;

	// ������ ��ġ�� ���� ��Ŷ�� ��ġ���� �ʹ� ū ���̰� �� ���, ��ũ ��Ŷ�� ���� ��ǥ ����
	if (abs(player->sectorX - shX) > dfERROR_RANGE || abs(player->sectorY - shY) > dfERROR_RANGE)
	{
		_log->logger(dfLOG_LEVEL_ERROR, __LINE__, L"# SYNC > Session ID: %d / StopDirection: %d / ServerX:%d / ServerY:%d / ClientX: %d / ClientY: %d ",
			player->sessionID, byDirection, player->sectorX, player->sectorY, shX, shY);

		CPacket* syncPacket = CPacket::Alloc();

		mpMySync(syncPacket, player->sessionID, player->sectorX, player->sectorY);

		// �� ���� ��ũ ����
		SendPacketAround(player, syncPacket, true);

		shX = player->sectorX;
		shY = player->sectorY;

		CPacket::Free(syncPacket);
	}

	// ���� ����. ������ -1
	player->dwAction = dfPACKET_MOVE_STOP;

	// ������ ����
	switch (byDirection)
	{
	case dfPACKET_MOVE_DIR_RR:
	case dfPACKET_MOVE_DIR_RU:
	case dfPACKET_MOVE_DIR_RD:
		player->byDirection = dfPACKET_MOVE_DIR_RR;
		break;
	case dfPACKET_MOVE_DIR_LL:
	case dfPACKET_MOVE_DIR_LU:
	case dfPACKET_MOVE_DIR_LD:
		player->byDirection = dfPACKET_MOVE_DIR_LL;
		break;
	}

	// Ŭ�󿡼� �� ��Ŷ(Ŭ���� ��ǥ ������)�� ������ �״�� ����
	player->sectorX = shX;
	player->sectorY = shY;

	int oldSize = _sector[player->OldSector.y][player->OldSector.x].size();
	int curSize = _sector[player->CurSector.y][player->CurSector.x].size();

		// ������ �ϸ鼭 ��ǥ�� �ణ ������ ���, ���� ������Ʈ�� ��
	if (SectorUpdateCharacter(player))
	{
		// ���Ͱ� ����� ���� Ŭ�󿡰� ���� ������ �� 
		CharacterSectorUpdatePacket(player);
	}

	CPacket* stopPacket = CPacket::Alloc();

	mpMoveStop(stopPacket, sessionID, player->byDirection, player->sectorX, player->sectorY);

	// ���� �ֺ� ���� ��ο��� ��Ŷ ���� (�� ����)
	SendPacketAround(player, stopPacket);

	CPacket::Free(stopPacket);

	return true;
}

bool MMOServer::NetPacketProc_Attack1(uint32_t sessionID, CPacket* packet)
{
	unsigned char byDirection;
	short shX;
	short shY;

	// player ã��
	Player* player = FindPlayer(sessionID);
	if (player == nullptr)
	{
		_log->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Attack1 # %016llx Player Not Found!", sessionID);
		CRASH();
	}

	*packet >> byDirection >> shX >> shY;

	// ������ ��ġ�� ���� ��Ŷ�� ��ġ���� �ʹ� ū ���̰� �� ���, ��ũ ��Ŷ�� ���� ��ǥ ����
	if (abs(player->sectorX - shX) > dfERROR_RANGE || abs(player->sectorY - shY) > dfERROR_RANGE)
	{
		_log->logger(dfLOG_LEVEL_ERROR, __LINE__, L"# SYNC > Session ID: %d / StopDirection: %d / ServerX:%d / ServerY:%d / ClientX: %d / ClientY: %d ",
			player->sessionID, byDirection, player->sectorX, player->sectorY, shX, shY);

		CPacket* syncPacket = CPacket::Alloc();

		mpMySync(syncPacket, player->sessionID, player->sectorX, player->sectorY);

		// �� ���� ��ũ ����
		SendPacketAround(player, syncPacket, true);

		shX = player->sectorX;
		shY = player->sectorY;

		CPacket::Free(syncPacket);
	}

	CPacket* attackPacket = CPacket::Alloc();

	// ������ ������ ä ����
	player->dwAction = dfPACKET_MOVE_STOP;

	// Ŭ�󿡼� �� ��Ŷ(Ŭ���� ��ǥ ������)�� ������ �״�� ����
	player->sectorX = shX;
	player->sectorY = shY;

	// ���� �������� ��� ����ڿ��� ���� ��Ŷ�� �Ѹ��� ���� ��Ŷ
	mpAttack1(attackPacket, sessionID, player->byDirection, player->sectorX, player->sectorY);
	
	// �ֺ� ���Ϳ� �����ϴ� Player�鿡�� ��Ŷ ���� (������ ���� ����)
	SendPacketAround(player, attackPacket);

	st_SECTOR_AROUND sectorAround;
	GetSectorAround(player->CurSector.x, player->CurSector.y, &sectorAround);

	// �ֺ� ���Ϳ��� ���� ���� ���� �ִ� ĳ���� �˻�
	for (int i = 0; i < sectorAround.iCount; i++)
	{
		auto iter = _sector[sectorAround.Around[i].y][sectorAround.Around[i].x].begin();

		// ����� ������ ����Ʈ�� ��ȸ�ϸ鼭 ���� ���� ���� �ִ� ĳ���� ã��
		for (; iter != _sector[sectorAround.Around[i].y][sectorAround.Around[i].x].end();)
		{
			Player* pExistPlayer = *iter;
			iter++;
		
			// �� �ڽ��� ���� ���� ����� �ƴϹǷ� ��ŵ
			if (pExistPlayer == player) continue;

			// �ڸ� �ٶ󺸰� �����ϸ� �ȵ� -> skip
			if (!isAttackBack(player, pExistPlayer)) continue;

			// ���� ���� ���� ������ ĳ���Ͱ� ������ ĳ���� ������ ��Ŷ �۽�
			if (abs(pExistPlayer->sectorX - shX) < dfATTACK1_RANGE_X &&
				abs(pExistPlayer->sectorY - shY) < dfATTACK1_RANGE_Y)
			{
				// ������ ->ü�� ����
				pExistPlayer->cHP -= dfATTACK1_DAMAGE;

				// HP�� 0 ������ ���, �� ����
				if (pExistPlayer->cHP <= 0)
					pExistPlayer->cHP = 0;

				CPacket* damagePacket = CPacket::Alloc();

				// ���� �������� ��� ����ڿ��� ������ ��Ŷ�� �Ѹ�
				mpDamage(damagePacket, sessionID, pExistPlayer->sessionID, pExistPlayer->cHP);

				// ������ �������� ���� ����
				SendPacketAround(pExistPlayer, damagePacket, true);

				CPacket::Free(damagePacket);
			}
		}
	}

	CPacket::Free(attackPacket);

	return true;
}

bool MMOServer::NetPacketProc_Attack2(uint32_t sessionID, CPacket* packet)
{
	unsigned char byDirection;
	short shX;
	short shY;

	// player ã��
	Player* player = FindPlayer(sessionID);
	if (player == nullptr)
	{
		_log->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Attack2 # %016llx Player Not Found!", sessionID);
		CRASH();
	}

	*packet >> byDirection >> shX >> shY;

	// ������ ��ġ�� ���� ��Ŷ�� ��ġ���� �ʹ� ū ���̰� �� ���, ��ũ ��Ŷ�� ���� ��ǥ ����
	if (abs(player->sectorX - shX) > dfERROR_RANGE || abs(player->sectorY - shY) > dfERROR_RANGE)
	{
		_log->logger(dfLOG_LEVEL_ERROR, __LINE__, L"# SYNC > Session ID: %d / StopDirection: %d / ServerX:%d / ServerY:%d / ClientX: %d / ClientY: %d ",
			player->sessionID, byDirection, player->sectorX, player->sectorY, shX, shY);

		CPacket* syncPacket = CPacket::Alloc();

		mpMySync(syncPacket, player->sessionID, player->sectorX, player->sectorY);

		// �� ���� ��ũ ����
		SendPacketAround(player, syncPacket, true);

		shX = player->sectorX;
		shY = player->sectorY;

		CPacket::Free(syncPacket);
	}

	// ������ ������ ä ����
	player->dwAction = dfPACKET_MOVE_STOP;

	// Ŭ�󿡼� �� ��Ŷ(Ŭ���� ��ǥ ������)�� ������ �״�� ����
	player->sectorX = shX;
	player->sectorY = shY;

	CPacket* attackPacket = CPacket::Alloc();

	// ���� �������� ��� ����ڿ��� ���� ��Ŷ�� �Ѹ��� ���� ��Ŷ
	mpAttack2(attackPacket, sessionID, player->byDirection, player->sectorX, player->sectorY);

	// �ֺ� ���Ϳ� �����ϴ� Player�鿡�� ��Ŷ ���� (������ ���� ����)
	SendPacketAround(player, attackPacket);

	CPacket::Free(attackPacket);

	// player�� �����ϴ� ������ �ֺ� 9�� ���� ���ϱ�
	st_SECTOR_AROUND sectorAround;
	GetSectorAround(player->CurSector.x, player->CurSector.y, &sectorAround);

	// �ֺ� ���Ϳ��� ���� ���� ���� �ִ� ĳ���� �˻�
	for (int i = 0; i < sectorAround.iCount; i++)
	{
		auto iter = _sector[sectorAround.Around[i].y][sectorAround.Around[i].x].begin();

		// ����� ������ ����Ʈ�� ��ȸ�ϸ鼭 ���� ���� ���� �ִ� ĳ���� ã��
		for (; iter != _sector[sectorAround.Around[i].y][sectorAround.Around[i].x].end();)
		{
			Player* pExistPlayer = *iter;
			iter++;

			// �� �ڽ��� ���� ���� ����� �ƴϹǷ� ��ŵ
			if (pExistPlayer == player)
				continue;

			// �ڸ� �ٶ󺸰� �����ϸ� �ȵ� -> skip
			if (!isAttackBack(player, pExistPlayer))
				continue;

			// ���� ���� ���� ������ ĳ���Ͱ� ������ ĳ���� ������ ��Ŷ �۽�
			if (abs(pExistPlayer->sectorX - shX) < dfATTACK2_RANGE_X &&
				abs(pExistPlayer->sectorY - shY) < dfATTACK2_RANGE_Y)
			{
				// ������
				pExistPlayer->cHP -= dfATTACK2_DAMAGE;

				// HP�� 0 ������ ���, �� ����
				if (pExistPlayer->cHP <= 0)
					pExistPlayer->cHP = 0;

				CPacket* damagePacket = CPacket::Alloc();

				// ���� �������� ��� ����ڿ��� ������ ��Ŷ�� �Ѹ�
				mpDamage(damagePacket, sessionID, pExistPlayer->sessionID, pExistPlayer->cHP);

				// ������ �������� ���� ����
				SendPacketAround(pExistPlayer, damagePacket, true);

				CPacket::Free(damagePacket);
			}
		}
	}

	return true;
}

bool MMOServer::NetPacketProc_Attack3(uint32_t sessionID, CPacket* packet)
{
	unsigned char byDirection;
	short shX;
	short shY;

	// player ã��
	Player* player = FindPlayer(sessionID);
	if (player == nullptr)
	{
		_log->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Attack3 # %016llx Player Not Found!", sessionID);
		CRASH();
	}

	*packet >> byDirection >> shX >> shY;

	// ������ ��ġ�� ���� ��Ŷ�� ��ġ���� �ʹ� ū ���̰� �� ���, ��ũ ��Ŷ�� ���� ��ǥ ����
	if (abs(player->sectorX - shX) > dfERROR_RANGE || abs(player->sectorY - shY) > dfERROR_RANGE)
	{
		_log->logger(dfLOG_LEVEL_ERROR, __LINE__, L"# SYNC > Session ID: %d / StopDirection: %d / ServerX:%d / ServerY:%d / ClientX: %d / ClientY: %d ",
			player->sessionID, byDirection, player->sectorX, player->sectorY, shX, shY);

		CPacket* syncPacket = CPacket::Alloc();

		mpMySync(syncPacket, player->sessionID, player->sectorX, player->sectorY);

		// �� ���� ��ũ ����
		SendPacketAround(player, syncPacket, true);

		shX = player->sectorX;
		shY = player->sectorY;

		CPacket::Free(syncPacket);
	}

	// ������ ������ ä ����
	player->dwAction = dfPACKET_MOVE_STOP;

	// Ŭ�󿡼� �� ��Ŷ(Ŭ���� ��ǥ ������)�� ������ �״�� ����
	player->sectorX = shX;
	player->sectorY = shY;

	CPacket* attackPacket = CPacket::Alloc();

	// ���� �������� ��� ����ڿ��� ���� ��Ŷ�� �Ѹ��� ���� ��Ŷ
	mpAttack3(attackPacket, sessionID, player->byDirection, player->sectorX, player->sectorY);

	// �ֺ� ���Ϳ� �����ϴ� Player�鿡�� ��Ŷ ���� (������ ���� ����)
	SendPacketAround(player, attackPacket);

	CPacket::Free(attackPacket);

	// player�� �����ϴ� ������ �ֺ� 9�� ���� ���ϱ�
	st_SECTOR_AROUND sectorAround;
	GetSectorAround(player->CurSector.x, player->CurSector.y, &sectorAround);

	// �ֺ� ���Ϳ��� ���� ���� ���� �ִ� ĳ���� �˻�
	for (int i = 0; i < sectorAround.iCount; i++)
	{
		auto iter = _sector[sectorAround.Around[i].y][sectorAround.Around[i].x].begin();

		// ����� ������ ����Ʈ�� ��ȸ�ϸ鼭 ���� ���� ���� �ִ� ĳ���� ã��
		for (; iter != _sector[sectorAround.Around[i].y][sectorAround.Around[i].x].end();)
		{
			Player* pExistPlayer = *iter;
			iter++;

			// �� �ڽ��� ���� ���� ����� �ƴϹǷ� ��ŵ
			if (pExistPlayer == player)
				continue;

			// �ڸ� �ٶ󺸰� �����ϸ� �ȵ� -> skip
			if (!isAttackBack(player, pExistPlayer))
				continue;

			// ���� ���� ���� ������ ĳ���Ͱ� ������ ĳ���� ������ ��Ŷ �۽�
			if (abs(pExistPlayer->sectorX - shX) < dfATTACK3_RANGE_X &&
				abs(pExistPlayer->sectorY - shY) < dfATTACK3_RANGE_Y)
			{
				// ������
				pExistPlayer->cHP -= dfATTACK3_DAMAGE;

				// HP�� 0 ������ ���, �� ����
				if (pExistPlayer->cHP <= 0)
					pExistPlayer->cHP = 0;

				CPacket* damagePacket = CPacket::Alloc();

				// ���� �������� ��� ����ڿ��� ������ ��Ŷ�� �Ѹ�
				mpDamage(damagePacket, sessionID, pExistPlayer->sessionID, pExistPlayer->cHP);

				// ������ �������� ���� ����
				SendPacketAround(pExistPlayer, damagePacket, true);

				CPacket::Free(damagePacket);
			}
		}
	}

	return true;
}


bool MMOServer::NetPacketProc_Echo(uint32_t sessionID, CPacket* packet)
{
	unsigned long echo;

	*packet >> echo;

	CPacket* echoPacket = CPacket::Alloc();

	mpEcho(echoPacket, echo);
	
	SendPacket(sessionID, echoPacket);

	CPacket::Free(echoPacket);

	return true;
}

// �����ڰ� �ݴ����� ���� �����ϴ��� �Ǵ�
bool MMOServer::isAttackBack(Player* Attacker, Player* Damaged)
{
	// �����ڰ� �ٶ󺸴� ������ ������ ���, �����ڴ� �� ������ ��ġ���� ���� ������ ������
	// �����ڰ� �ٶ󺸴� ������ ������ ���, �����ڴ� �� ���� ��ġ���� �ݴ� ������ ������ (���� ���ֺ��� ����)
	// �����ڰ� �ٶ󺸴� ������ �������� ���, �����ڴ� �� ���� ��ġ���� ���� ������ ������
	// �����ڰ� �ٶ󺸴� ������ �������� ���, �����ڴ� �� ������ ��ġ���� �ݴ� ������ ������ (���� ���ֺ��� ����)
	if (dfPACKET_MOVE_DIR_LL == Damaged->byDirection && 
		dfPACKET_MOVE_DIR_LL == Attacker->byDirection &&
		Attacker->sectorX > Damaged->sectorX)
		return true;

	else if (dfPACKET_MOVE_DIR_LL == Damaged->byDirection && 
		dfPACKET_MOVE_DIR_RR == Attacker->byDirection && 
		Attacker->sectorX < Damaged->sectorX)
		return true;

	else if (dfPACKET_MOVE_DIR_RR == Damaged->byDirection && 
		dfPACKET_MOVE_DIR_RR == Attacker->byDirection && 
		Attacker->sectorX < Damaged->sectorX)
		return true;

	else if (dfPACKET_MOVE_DIR_RR == Damaged->byDirection && 
		dfPACKET_MOVE_DIR_LL == Attacker->byDirection && 
		Attacker->sectorX > Damaged->sectorX)
		return true;

	return false;
}

// ���Ϳ� ��ȭ�� ������ ��쿡�� ����
void MMOServer::CharacterSectorUpdatePacket(Player* player)
{
	st_SECTOR_AROUND RemoveSector, AddSector;

	RemoveSector.iCount = AddSector.iCount = 0;

	int iCnt;

	CPacket* delPacket = CPacket::Alloc();

	// ������ ���� & �߰��� ���� ����
	GetUpdateSectorAround(player, &RemoveSector, &AddSector);

	// 1. RemoveSector�� ĳ���� ���� ��Ŷ ������
	mpDelete(delPacket, player->sessionID);

	// 2. ���� �����̴� ��������, RemoveSector�� ĳ���͵� ���� ��Ŷ ������
	for (iCnt = 0; iCnt < RemoveSector.iCount; iCnt++)
	{
		// Ư������ �� �������� �޽����� �����ϴ� �Լ�
		SendPacketSectorOne(RemoveSector.Around[iCnt].x, RemoveSector.Around[iCnt].y, delPacket, nullptr);

		// ����� ���͸� ���鼭 ���͸���Ʈ ����
		auto iter = _sector[RemoveSector.Around[iCnt].y][RemoveSector.Around[iCnt].x].begin();

		// �ش� ���͸��� ��ϵ� ĳ���͵��� �̾� ������Ŷ ����� ����
		for (; iter != _sector[RemoveSector.Around[iCnt].y][RemoveSector.Around[iCnt].x].end();)
		{
			Player* pExistPlayer = *iter;
			++iter;

			CPacket* delOtherPacket = CPacket::Alloc();

			mpDelete(delOtherPacket, pExistPlayer->sessionID);
			SendPacket(player->sessionID, delOtherPacket);

			CPacket::Free(delOtherPacket);
		}
	}

	CPacket::Free(delPacket);

	CPacket* myPacket = CPacket::Alloc();
	CPacket* myMovePacket = CPacket::Alloc();

	// 3. AddSector�� ĳ���� ���� ��Ŷ ������
	mpCreateOther(myPacket, player->sessionID, player->byDirection, player->sectorX, player->sectorX, player->cHP);

	// 3-1. AddSector�� ������ ĳ���� �̵� ��Ŷ ������
	mpMoveStart(myMovePacket, player->sessionID, player->dwAction, player->sectorX, player->sectorY);

	for (iCnt = 0; iCnt < AddSector.iCount; iCnt++)
	{
		// Ư������ �� �������� �޽����� �����ϴ� �Լ�
		SendPacketSectorOne(AddSector.Around[iCnt].x, AddSector.Around[iCnt].y, myPacket, nullptr);
		SendPacketSectorOne(AddSector.Around[iCnt].x, AddSector.Around[iCnt].y, myMovePacket, nullptr);

		// 4. �̵��� ĳ���Ϳ��� AddSector�� �ִ� ĳ���͵� ���� ��Ŷ ������
		auto iter = _sector[AddSector.Around[iCnt].y][AddSector.Around[iCnt].x].begin();

		// �ش� ���͸��� ��ϵ� ĳ���͵��� �̾� ������Ŷ ����� ����
		for (; iter != _sector[AddSector.Around[iCnt].y][AddSector.Around[iCnt].x].end();)
		{
			Player* pExistPlayer = *iter;
			++iter;
			if (pExistPlayer == player) continue;
			CPacket* packetOther = CPacket::Alloc();

			// �� �ֺ� ĳ����
			mpCreateOther(packetOther, pExistPlayer->sessionID,	pExistPlayer->byDirection, pExistPlayer->sectorX, pExistPlayer->sectorY, pExistPlayer->cHP);

			// ������ �ֺ� ������ ĳ���� ���� ����
			SendPacket(player->sessionID, packetOther);
			
			CPacket::Free(packetOther);

			// �� AddSector�� ĳ���Ͱ� �Ȱ� �־��ٸ� �̵� ��Ŷ ����� ����
			switch (pExistPlayer->dwAction)
			{
			case dfPACKET_MOVE_DIR_LL:
			case dfPACKET_MOVE_DIR_LU:
			case dfPACKET_MOVE_DIR_UU:
			case dfPACKET_MOVE_DIR_RU:
			case dfPACKET_MOVE_DIR_RR:
			case dfPACKET_MOVE_DIR_RD:
			case dfPACKET_MOVE_DIR_DD:
			case dfPACKET_MOVE_DIR_LD:
			{
				CPacket* otherMovePacket = CPacket::Alloc();
				mpMoveStart(otherMovePacket, pExistPlayer->sessionID, pExistPlayer->dwAction, pExistPlayer->sectorX, pExistPlayer->sectorY);
				SendPacket(player->sessionID, otherMovePacket);
				CPacket::Free(otherMovePacket);
			}
			break;
			}
		}
	}

	CPacket::Free(myPacket);
	CPacket::Free(myMovePacket);
}

void MMOServer::SendPacketAround(Player* player, CPacket* packet, bool bSendMe)
{
	// player�� �����ϴ� ������ �ֺ� 9�� ���� ���ϱ�
	st_SECTOR_AROUND sectorAround;
	GetSectorAround(player->CurSector.x, player->CurSector.x, &sectorAround);

	// ���� ���� ���� (���� �������� �ֺ� ���� ���� ��� ����ڿ��� ��Ŷ�� �Ѹ�)
	if (bSendMe)
	{
		for (int i = 0; i < sectorAround.iCount; i++)
		{
			auto iter = _sector[sectorAround.Around[i].y][sectorAround.Around[i].x].begin();
			for (; iter != _sector[sectorAround.Around[i].y][sectorAround.Around[i].x].end();)
			{
				Player* otherPlayer = *iter;
				++iter;

				SendPacket(otherPlayer->sessionID, packet);
			}
		}
	}
	// ���� ���� ����
	else
	{
		for (int i = 0; i < sectorAround.iCount; i++)
		{
			auto iter = _sector[sectorAround.Around[i].y][sectorAround.Around[i].x].begin();
			for (; iter != _sector[sectorAround.Around[i].y][sectorAround.Around[i].x].end();)
			{
				Player* otherPlayer = *iter;
				++iter;

				if (otherPlayer == player)
					continue;

				SendPacket(otherPlayer->sessionID, packet);
			}
		}
	}
}

// Ư�� ���� 1���� �ִ� Ŭ���̾�Ʈ�鿡�� �޽��� ������
void MMOServer::SendPacketSectorOne(int iSectorX, int iSectorY, CPacket* packet, Player* pExcepPlayer)
{
	auto iter = _sector[iSectorY][iSectorX].begin();

	// Ư�� ���Ϳ� �ִ� ĳ���͵鿡�� �޽��� send
	for (; iter != _sector[iSectorY][iSectorX].end();)
	{
		Player* curPlayer = *iter;
		++iter;

		// ������ ĳ���� ������ ��ŵ
		if (pExcepPlayer != nullptr && curPlayer == pExcepPlayer)
			continue;

		// �� ĳ���� ���ǿ� send
		SendPacket(curPlayer->sessionID, packet);
	}
}


// --------------------------------------------------------------------------------
// Rander Update
// --------------------------------------------------------------------------------
bool MMOServer::UpdateSkip()
{
	// Update() ȣ�� Ÿ�̹� Ȯ�ο�
	static DWORD oldTick = timeGetTime();
	DWORD curTick = timeGetTime();

	// �����ؾ��� ������
	static DWORD recoveryFrame = 0;

	// FPS ��� �뵵
	static DWORD frameCheckOldTick = timeGetTime();

	// ���� ������
	static DWORD updateFPS = 0;

	// ������ ������
	static DWORD recoveryUpdateFPS = 0;

	// 1�ʸ��� FPS ���
	if (curTick - frameCheckOldTick >= 1000)
	{
		// �������� ���� ������ + ������ ������
		frameCheckOldTick += 1000;

		// updateFPS�� ��ǥ ������(25fps)���� ���ٸ� ���� �ؾ��� �������� �ִٴ� �ǹ�
		if (updateFPS < 25)
		{
			// ���� �ؾ��� �����ӿ� �߰�
			recoveryFrame += (25 - updateFPS);
		}

		// 1�� �������Ƿ� FPS �ʱ�ȭ
		updateFPS = 0;
		recoveryUpdateFPS = 0;
	}

	// 40ms ���� ������ �ѹ��� Update ȣ��
	if (curTick - oldTick >= 40)
	{
		oldTick += 40;
		updateFPS++;
		return true;
	}

	// ���� �ؾ��� �������� �ִٸ� �ٷ� update ȣ��
	if (recoveryFrame > 0)
	{
		// �����ؾ��� ������ 1 ����
		recoveryFrame--;

		// ������ ������ 1 ����
		recoveryUpdateFPS++;
		return true;
	}

	return false;
}

void MMOServer::Update()
{
	// ������ ���� ������Ʈ ó�� Ÿ�̹� ��� (25 fps)
	// 25 �����Ӹ��� ������Ʈ
	if (!UpdateSkip())
		return;

	DWORD curTicks = timeGetTime();

	// ���� �����ϴ� ��� �÷��̾�鿡 ���� ������Ʈ ����
	for (auto iter = _mapPlayer.begin(); iter != _mapPlayer.end();)
	{
		Player* player = iter->second;
		iter++;
		
		if (player->cHP == 0)
		{
			// ��� ó��
			DisconnectSession(player->sessionID);
			continue;
		}

		// ���� ���ۿ� ���� ó��
		switch (player->dwAction)
		{
		case dfPACKET_MOVE_STOP:

			break;
		case dfPACKET_MOVE_DIR_LL:
		{
			// ���⿡ ���� �̵�ó��
			if (player->sectorX - dfSPEED_PLAYER_X > dfRANGE_MOVE_LEFT)
			{
				player->sectorX -= dfSPEED_PLAYER_X;
			}
			break;
		}
		case dfPACKET_MOVE_DIR_LU:
		{
			// ���⿡ ���� �̵�ó��
			// x�� ���� ȭ�� ���� �Ѿ ��� y ��ǥ�� �ǵ�� �ȵ�
			if (player->sectorX - dfSPEED_PLAYER_X > dfRANGE_MOVE_LEFT && player->sectorY - dfSPEED_PLAYER_Y > dfRANGE_MOVE_TOP)
			{
				player->sectorX -= dfSPEED_PLAYER_X;
				player->sectorY -= dfSPEED_PLAYER_Y;
			}

			break;
		}
		case dfPACKET_MOVE_DIR_UU:
		{
			// ���⿡ ���� �̵�ó��
			if (player->sectorY - dfSPEED_PLAYER_X > dfRANGE_MOVE_TOP)
			{
				player->sectorY -= dfSPEED_PLAYER_Y;
			}
			break;
		}
		case dfPACKET_MOVE_DIR_RU:
		{
			// ���⿡ ���� �̵�ó��
			if (player->sectorX + dfSPEED_PLAYER_X < dfRANGE_MOVE_RIGHT && player->sectorY - dfSPEED_PLAYER_Y > dfRANGE_MOVE_TOP)
			{
				player->sectorX += dfSPEED_PLAYER_X;
				player->sectorY -= dfSPEED_PLAYER_Y;
			}
			break;
		}
		case dfPACKET_MOVE_DIR_RR:
		{
			// ���⿡ ���� �̵�ó��
			if (player->sectorX + dfSPEED_PLAYER_X < dfRANGE_MOVE_RIGHT)
			{
				player->sectorX += dfSPEED_PLAYER_X;
			}
			break;
		}
		case dfPACKET_MOVE_DIR_RD:
		{
			// ���⿡ ���� �̵�ó��
			if (player->sectorX + dfSPEED_PLAYER_X < dfRANGE_MOVE_RIGHT && player->sectorY + dfSPEED_PLAYER_Y < dfRANGE_MOVE_BOTTOM)
			{
				player->sectorX += dfSPEED_PLAYER_X;
				player->sectorY += dfSPEED_PLAYER_Y;
			}
			break;
		}
		case dfPACKET_MOVE_DIR_DD:
		{
			// ���⿡ ���� �̵�ó��
			if (player->sectorY + dfSPEED_PLAYER_Y < dfRANGE_MOVE_BOTTOM)
			{
				player->sectorY += dfSPEED_PLAYER_Y;
			}
			break;
		}
		case dfPACKET_MOVE_DIR_LD:
		{
			// ���⿡ ���� �̵�ó��
			if (player->sectorX - dfSPEED_PLAYER_X > dfRANGE_MOVE_LEFT && player->sectorY + dfSPEED_PLAYER_Y < dfRANGE_MOVE_BOTTOM)
			{
				player->sectorX -= dfSPEED_PLAYER_X;
				player->sectorY += dfSPEED_PLAYER_Y;
			}
			break;
		}
		default:
			continue;
		}

		// �̵��ϰ� ���� ���
		if (player->dwAction != dfPACKET_MOVE_STOP)
		{
			// ���� ������Ʈ
			if (SectorUpdateCharacter(player))
			{
				// ���Ͱ� ����� ���� Ŭ�󿡰� ���� ���� ����
				CharacterSectorUpdatePacket(player);
			}
		}
	}
}

// ------------------------------------------------------------------------
// Sector
// ------------------------------------------------------------------------
// 
// Ư�� ���� ��ǥ �������� �ֺ� �ִ� 9���� ���� ��ǥ�� ����
void MMOServer::GetSectorAround(int iSectorX, int iSectorY, st_SECTOR_AROUND* pSectorAround)
{
	// ���� �� ���Ϳ��� ������ܺ��� ���� ��ǥ�� ��� ����
	iSectorX--;
	iSectorY--;

	pSectorAround->iCount = 0;

	for (int iY = 0; iY < 3; iY++)
	{
		// �����ڸ��� ��ġ�� ���� ���, ���� ������ ����� �͵��� ����
		if (iSectorY + iY < 0 || iSectorY + iY >= dfSECTOR_Y_MAX)
			continue;

		for (int iX = 0; iX < 3; iX++)
		{
			if (iSectorX + iX < 0 || iSectorX + iX >= dfSECTOR_X_MAX)
				continue;

			pSectorAround->Around[pSectorAround->iCount].x = iSectorX + iX;
			pSectorAround->Around[pSectorAround->iCount].y = iSectorY + iY;
			++pSectorAround->iCount;
		}
	}
}

// ĳ���Ͱ� ���͸� �̵� �� ��, ����ǿ��� ���� ����(������ ����),, ���� �߰��� ������ ���� ���ϴ� �Լ�
void MMOServer::GetUpdateSectorAround(Player* pCharacter, st_SECTOR_AROUND* pRemoveSector, st_SECTOR_AROUND* pAddSector)
{
	// ��ǥ �̵� �� ���� ���ϱ�
	st_SECTOR_AROUND oldSector;
	GetSectorAround(pCharacter->OldSector.x, pCharacter->OldSector.y, &oldSector);

	// ���� �� ���� ���ϱ�
	st_SECTOR_AROUND CurSector;
	GetSectorAround(pCharacter->CurSector.x, pCharacter->CurSector.y, &CurSector);

	bool flag = false;

	pRemoveSector->iCount = 0;

	// oldSector���� ��ġ�� ���� ������ �κ��� removeSector
	for (int i = 0; i < oldSector.iCount; i++)
	{
		flag = false;

		for (int j = 0; j < CurSector.iCount; j++)
		{
			// ��ġ�� �κ��� ������ ���Ͱ� �ƴ�
			if (oldSector.Around[i].x == CurSector.Around[j].x && oldSector.Around[i].y == CurSector.Around[j].y)
			{
				flag = true;
				break;
			}
		}

		if (!flag)
		{
			pRemoveSector->Around[pRemoveSector->iCount++] = oldSector.Around[i];
		}
	}

	pAddSector->iCount = 0;

	// CurSector���� ��ġ�� ���� ������ �κ��� addSector
	for (int i = 0; i < CurSector.iCount; i++)
	{
		flag = false;

		for (int j = 0; j < oldSector.iCount; j++)
		{
			// ��ġ�� �κ��� �߰��� ���Ͱ� �ƴ�
			if (CurSector.Around[i].x == oldSector.Around[j].x && CurSector.Around[i].y == oldSector.Around[j].y)
			{
				flag = true;
				break;
			}
		}

		if (!flag)
		{
			pAddSector->Around[pAddSector->iCount++] = CurSector.Around[i];
		}
	}
}

// ĳ������ ���� ��ǥ x, y�� ���� ������ ��ǥ�� ������ ����, �����ϴ� �Լ�
bool MMOServer::SectorUpdateCharacter(Player* pCharacter)
{
	int curX;
	int curY;

	// ��ǥ ����
	if (pCharacter->sectorX >= dfRANGE_MOVE_RIGHT)
		curX = dfRANGE_MOVE_RIGHT - 1;
	else if (pCharacter->sectorX < 0)
		curX = 0;
	else
		curX = pCharacter->sectorX;

	if (pCharacter->sectorY >= dfRANGE_MOVE_BOTTOM)
		curY = dfRANGE_MOVE_BOTTOM - 1;
	else if (pCharacter->sectorY < 0)
		curY = 0;
	else
		curY = pCharacter->sectorY;

	st_SECTOR_POS sectorPos;
	sectorPos.x = curX / SECTOR_SIZE_X;
	sectorPos.y = curY / SECTOR_SIZE_Y;

	// ���� ���� ��ǥ�� ���� ���� ��ǥ�� ���Ͽ� ����ƴ��� Ȯ��
	if (pCharacter->CurSector.x == sectorPos.x && pCharacter->CurSector.y == sectorPos.y)
		return false;

	pCharacter->OldSector = pCharacter->CurSector;

	auto iter = _sector[pCharacter->OldSector.y][pCharacter->OldSector.x].find(pCharacter);

	// ���ŵ� ���ߴµ�, set �ȿ� ���ٴ� �� ���� �ȵ� - error
	if (iter == _sector[pCharacter->OldSector.y][pCharacter->OldSector.x].end())
	{
		return false;
	}

	// ���� ���Ϳ��� �ش� �÷��̾� ����
	_sector[pCharacter->OldSector.y][pCharacter->OldSector.x].erase(pCharacter);

	pCharacter->CurSector = sectorPos;

	// ���� ���Ϳ��� �ش� �÷��̾� �߰�
	_sector[pCharacter->CurSector.y][pCharacter->CurSector.x].insert(pCharacter);

	return true;
}
