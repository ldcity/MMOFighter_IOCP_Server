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

	// login server 설정파일을 Parsing하여 읽어옴
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
		// 1초마다 모니터링 -> 타임아웃 건도 처리
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

		// 업데이트 (25fps)
		Update();

		// Job이 없을 때까지 update 반복
		while (_jobQ.GetSize() > 0)
		{
			if (_jobQ.Dequeue(job))
			{
				// Job Type에 따른 분기 처리
				switch (job->type)
				{
				case JobType::NEW_CONNECT:
					CreatePlayer(job->sessionID);				// Player 생성
					break;

				case JobType::DISCONNECT:
					DeletePlayer(job->sessionID);				// Player 삭제
					break;

				case JobType::MSG_PACKET:
					PacketProc(job->sessionID, job->packet);	// 패킷 처리
					break;

				case JobType::TIMEOUT:
					DisconnectSession(job->sessionID);			// 타임 아웃
					break;

				default:
					DisconnectSession(job->sessionID);
					break;
				}

				// 접속, 해제 Job은 packet이 nullptr이기 때문에 반환할 Packet이 없음
				if (job->packet != nullptr)
					CPacket::Free(job->packet);

				// JobPool에 Job 객체 반환
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


// 접속 처리 - 이 때 Player 껍데기를 미리 만들어놔야함
// 로그인 전에 연결만 된 세션에 대해 timeout도 판단해야하기 때문에
void MMOServer::OnClientJoin(uint32_t sessionID)
{
	if (!_startFlag)
	{
		ResumeThread(_moniteringThread);
		_startFlag = true;
	}

	Job* job = _jobPool.Alloc();			// jobPool에서 job 할당
	job->type = JobType::NEW_CONNECT;		// 새 접속 상태
	job->sessionID = sessionID;				// Player에 부여할 SessionID
	job->packet = nullptr;					// Player 생성 시에는 패킷 필요 없음

	_jobQ.Enqueue(job);					// JobQ에 Enqueue
	//SetEvent(_jobEvent);					// Job Worker Thread Event 발생
}

// 해제 처리
void MMOServer::OnClientLeave(uint32_t sessionID)
{
	Job* job = _jobPool.Alloc();
	job->type = JobType::DISCONNECT;
	job->sessionID = sessionID;
	job->packet = nullptr;

	_jobQ.Enqueue(job);
	//SetEvent(_jobEvent);
}

// 패킷 처리
void MMOServer::OnRecv(uint32_t sessionID, CPacket* packet)
{
	Job* job = _jobPool.Alloc();
	job->type = JobType::MSG_PACKET;
	job->sessionID = sessionID;
	job->packet = packet;

	_jobQ.Enqueue(job);
	//SetEvent(_jobEvent);
}

// Network Logic 으로부터 timeout 처리가 발생되면 timeout Handler 호출
void MMOServer::OnTimeout(uint32_t sessionID)
{

}

void MMOServer::OnError(int errorCode, const wchar_t* msg)
{

}

// player 생성 & 주변 업데이트 패킷 전송
bool MMOServer::CreatePlayer(uint32_t sessionID)
{
	Player* player = _playerPool.Alloc();

	if (player == nullptr)
	{
		_log->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Player Pool Alloc Failed!");

		CRASH();

		return false;
	}

	player->sessionID = sessionID;					// sessionID 셋팅
	player->dwAction = dfPACKET_MOVE_STOP;
	player->byDirection = rand() % 2 ? dfPACKET_MOVE_DIR_LL : dfPACKET_MOVE_DIR_RR;
	player->sectorX = rand() % 1000;
	player->sectorY = rand() % 1000;

	player->cHP = 100;
	player->OldSector.x = player->sectorX / SECTOR_SIZE_X;
	player->OldSector.y = player->sectorY / SECTOR_SIZE_Y;
	player->CurSector.x = player->sectorX / SECTOR_SIZE_X;
	player->CurSector.y = player->sectorY / SECTOR_SIZE_Y;

	_mapPlayer.insert({ sessionID, player });		// 전체 Player를 관리하는 map에 insert
	_sector[player->CurSector.y][player->CurSector.x].insert(player);

	InterlockedIncrement64(&_playerCnt);
	InterlockedIncrement64(&_playerTPS);

	CPacket* myPacket = CPacket::Alloc();
	CPacket* otherPacket = CPacket::Alloc();
	CPacket* otherPacket2 = CPacket::Alloc();
	CPacket* movePacket = CPacket::Alloc();

	// 새로운 플레이어 패킷
	mpCreateMe(myPacket, player->sessionID, player->byDirection, player->sectorX, player->sectorY, player->cHP);
	SendPacket(sessionID, myPacket);

	// 주변 섹터의 다른 플레이어들에게 새로운 플레이어 패킷 전송
	mpCreateOther(otherPacket, player->sessionID, player->byDirection, player->sectorX, player->sectorY, player->cHP);
	SendPacketAround(player, otherPacket);

	// 주변 섹터 범위 계산
	st_SECTOR_AROUND sectorAround;
	GetSectorAround(player->CurSector.x, player->CurSector.y, &sectorAround);

	// 다른 플레이어들의 정보 패킷들을 새로 입장하는 플레이어에게 전송
	for (int i = 0; i < sectorAround.iCount; i++)
	{
		auto iter = _sector[sectorAround.Around[i].y][sectorAround.Around[i].x].begin();
		for (; iter != _sector[sectorAround.Around[i].y][sectorAround.Around[i].x].end();)
		{
			Player* pExistPlayer = *iter;
			++iter;

			// 내 플레이어 정보인 경우 제외 (섹터 범위 내에 있는 다른 플레이어들의 정보 패킷을 나한테 보내는 것이므로)
			if (pExistPlayer == player)
				continue;

			// 다른 플레이어 생성 정보 전송
			mpCreateOther(otherPacket2, pExistPlayer->sessionID,
				pExistPlayer->byDirection,
				pExistPlayer->sectorX,
				pExistPlayer->sectorY, 
				pExistPlayer->cHP);
			SendPacket(sessionID, otherPacket2);

			// 다른 섹터의 캐릭터가 이동 중이었다면 이동 패킷 만들어서 전송
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

// player 삭제
bool MMOServer::DeletePlayer(uint32_t sessionID)
{
	// player 검색
	Player* player = FindPlayer(sessionID);

	if (player == nullptr)
	{
		_log->logger(dfLOG_LEVEL_DEBUG, __LINE__, L"DeletePlayer # Player Not Found! ID : %016llx", sessionID);
		CRASH();
		return false;
	}

	CPacket* deletePacket = CPacket::Alloc();

	// 섹터에서 해당 player 객체 삭제
	_sector[player->CurSector.y][player->CurSector.x].erase(player);

	// 주변 섹터에 종료 패킷 전송 (본인 제외 전송)
	mpDelete(deletePacket, sessionID);
	SendPacketAround(player, deletePacket);

	// 전체 Player 관리 map에서 해당 player 삭제
	_mapPlayer.erase(player->sessionID);								

	InterlockedDecrement64(&_playerCnt);

	CPacket::Free(deletePacket);
	_playerPool.Free(player);		// PlayerPool에 player 반환

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
		DisconnectSession(sessionID);					// 세션 타임아웃
		break;

	default:
		// 잘못된 패킷
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

	// player 찾기
	Player* player = FindPlayer(sessionID);
	if (player == nullptr)
	{
		_log->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Start # %016llx Player Not Found!", sessionID);
		return false;
	}

	*packet >> byDirection >> shX >> shY;

	CPacket* movePacket = CPacket::Alloc();

	// 서버의 위치와 받은 패킷의 위치값이 너무 큰 차이가 날 경우, 싱크 패킷을 보내 좌표 보정
	if (abs(player->sectorX - shX) > dfERROR_RANGE || abs(player->sectorY - shY) > dfERROR_RANGE)
	{
		_log->logger(dfLOG_LEVEL_ERROR, __LINE__, L"# SYNC > Session ID: %d / MoveDirection: %d / ServerX:%d / ServerY:%d / ClientX: %d / ClientY: %d ",
			player->sessionID, byDirection, player->sectorX, player->sectorY, shX, shY);

		CPacket* syncPacket = CPacket::Alloc();

		mpMySync(syncPacket, player->sessionID, player->sectorX, player->sectorY);

		// player가 존재하는 섹터의 주변 9개 섹터 구한 뒤 싱크 패킷 전송
		SendPacketAround(player, syncPacket, true);

		shX = player->sectorX;
		shY = player->sectorY;

		CPacket::Free(syncPacket);
	}

	// 동작을 변경. 동작번호와, 방향값이 같음
	player->dwAction = byDirection;

	// 바라보는 방향을 변경
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

	// 클라에서 온 패킷(클라의 좌표 데이터)을 서버가 그대로 믿음 
	// -> 좌표 틀어지면 보정된 값 셋팅
	player->sectorX = shX;
	player->sectorY = shY;

	// 이전 섹터의 크기와 이동 후에 현재 섹터의 크기
	int oldSize = _sector[player->OldSector.y][player->OldSector.x].size();
	int curSize = _sector[player->CurSector.y][player->CurSector.x].size();

	// 정지를 하면서 좌표가 약간 조절된 경우, 섹터 업테이트를 함
	if (SectorUpdateCharacter(player))
	{
		// 섹터가 변경된 경우는 클라에게 관련 정보를 쏨 
		CharacterSectorUpdatePacket(player);
	}

	// 이동 시작 패킷
	mpMoveStart(movePacket, sessionID, player->dwAction, player->sectorX, player->sectorY);

	// 현재 주변 섹터 모두에게 패킷 전송
	SendPacketAround(player, movePacket);

	CPacket::Free(movePacket);

	return true;
}

bool MMOServer::NetPacketProc_MoveStop(uint32_t sessionID, CPacket* packet)
{
	unsigned char byDirection;
	short shX;
	short shY;

	// player 찾기
	Player* player = FindPlayer(sessionID);
	if (player == nullptr)
	{
		_log->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Stop # %016llx Player Not Found!", sessionID);
		CRASH();
	}

	*packet >> byDirection >> shX >> shY;

	// 서버의 위치와 받은 패킷의 위치값이 너무 큰 차이가 날 경우, 싱크 패킷을 보내 좌표 보정
	if (abs(player->sectorX - shX) > dfERROR_RANGE || abs(player->sectorY - shY) > dfERROR_RANGE)
	{
		_log->logger(dfLOG_LEVEL_ERROR, __LINE__, L"# SYNC > Session ID: %d / StopDirection: %d / ServerX:%d / ServerY:%d / ClientX: %d / ClientY: %d ",
			player->sessionID, byDirection, player->sectorX, player->sectorY, shX, shY);

		CPacket* syncPacket = CPacket::Alloc();

		mpMySync(syncPacket, player->sessionID, player->sectorX, player->sectorY);

		// 나 포함 싱크 보정
		SendPacketAround(player, syncPacket, true);

		shX = player->sectorX;
		shY = player->sectorY;

		CPacket::Free(syncPacket);
	}

	// 동작 변경. 정지는 -1
	player->dwAction = dfPACKET_MOVE_STOP;

	// 방향을 변경
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

	// 클라에서 온 패킷(클라의 좌표 데이터)을 서버가 그대로 믿음
	player->sectorX = shX;
	player->sectorY = shY;

	int oldSize = _sector[player->OldSector.y][player->OldSector.x].size();
	int curSize = _sector[player->CurSector.y][player->CurSector.x].size();

		// 정지를 하면서 좌표가 약간 조절된 경우, 섹터 업테이트를 함
	if (SectorUpdateCharacter(player))
	{
		// 섹터가 변경된 경우는 클라에게 관련 정보를 쏨 
		CharacterSectorUpdatePacket(player);
	}

	CPacket* stopPacket = CPacket::Alloc();

	mpMoveStop(stopPacket, sessionID, player->byDirection, player->sectorX, player->sectorY);

	// 현재 주변 섹터 모두에게 패킷 전송 (나 제외)
	SendPacketAround(player, stopPacket);

	CPacket::Free(stopPacket);

	return true;
}

bool MMOServer::NetPacketProc_Attack1(uint32_t sessionID, CPacket* packet)
{
	unsigned char byDirection;
	short shX;
	short shY;

	// player 찾기
	Player* player = FindPlayer(sessionID);
	if (player == nullptr)
	{
		_log->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Attack1 # %016llx Player Not Found!", sessionID);
		CRASH();
	}

	*packet >> byDirection >> shX >> shY;

	// 서버의 위치와 받은 패킷의 위치값이 너무 큰 차이가 날 경우, 싱크 패킷을 보내 좌표 보정
	if (abs(player->sectorX - shX) > dfERROR_RANGE || abs(player->sectorY - shY) > dfERROR_RANGE)
	{
		_log->logger(dfLOG_LEVEL_ERROR, __LINE__, L"# SYNC > Session ID: %d / StopDirection: %d / ServerX:%d / ServerY:%d / ClientX: %d / ClientY: %d ",
			player->sessionID, byDirection, player->sectorX, player->sectorY, shX, shY);

		CPacket* syncPacket = CPacket::Alloc();

		mpMySync(syncPacket, player->sessionID, player->sectorX, player->sectorY);

		// 나 포함 싱크 보정
		SendPacketAround(player, syncPacket, true);

		shX = player->sectorX;
		shY = player->sectorY;

		CPacket::Free(syncPacket);
	}

	CPacket* attackPacket = CPacket::Alloc();

	// 공격은 정지한 채 진행
	player->dwAction = dfPACKET_MOVE_STOP;

	// 클라에서 온 패킷(클라의 좌표 데이터)을 서버가 그대로 믿음
	player->sectorX = shX;
	player->sectorY = shY;

	// 현재 접속중인 모든 사용자에게 공격 패킷을 뿌리기 위한 패킷
	mpAttack1(attackPacket, sessionID, player->byDirection, player->sectorX, player->sectorY);
	
	// 주변 섹터에 존재하는 Player들에게 패킷 전송 (공격자 제외 전송)
	SendPacketAround(player, attackPacket);

	st_SECTOR_AROUND sectorAround;
	GetSectorAround(player->CurSector.x, player->CurSector.y, &sectorAround);

	// 주변 섹터에서 공격 범위 내에 있는 캐릭터 검사
	for (int i = 0; i < sectorAround.iCount; i++)
	{
		auto iter = _sector[sectorAround.Around[i].y][sectorAround.Around[i].x].begin();

		// 얻어진 섹터의 리스트를 순회하면서 공격 범위 내에 있는 캐릭터 찾음
		for (; iter != _sector[sectorAround.Around[i].y][sectorAround.Around[i].x].end();)
		{
			Player* pExistPlayer = *iter;
			iter++;
		
			// 나 자신은 공격 범위 대상이 아니므로 스킵
			if (pExistPlayer == player) continue;

			// 뒤를 바라보고 공격하면 안됨 -> skip
			if (!isAttackBack(player, pExistPlayer)) continue;

			// 공격 범위 내에 피해자 캐릭터가 있으면 캐릭터 데미지 패킷 송신
			if (abs(pExistPlayer->sectorX - shX) < dfATTACK1_RANGE_X &&
				abs(pExistPlayer->sectorY - shY) < dfATTACK1_RANGE_Y)
			{
				// 데미지 ->체력 감소
				pExistPlayer->cHP -= dfATTACK1_DAMAGE;

				// HP가 0 이하일 경우, 값 보정
				if (pExistPlayer->cHP <= 0)
					pExistPlayer->cHP = 0;

				CPacket* damagePacket = CPacket::Alloc();

				// 현재 접속중인 모든 사용자에게 데미지 패킷을 뿌림
				mpDamage(damagePacket, sessionID, pExistPlayer->sessionID, pExistPlayer->cHP);

				// 피해자 기준으로 섹터 전송
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

	// player 찾기
	Player* player = FindPlayer(sessionID);
	if (player == nullptr)
	{
		_log->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Attack2 # %016llx Player Not Found!", sessionID);
		CRASH();
	}

	*packet >> byDirection >> shX >> shY;

	// 서버의 위치와 받은 패킷의 위치값이 너무 큰 차이가 날 경우, 싱크 패킷을 보내 좌표 보정
	if (abs(player->sectorX - shX) > dfERROR_RANGE || abs(player->sectorY - shY) > dfERROR_RANGE)
	{
		_log->logger(dfLOG_LEVEL_ERROR, __LINE__, L"# SYNC > Session ID: %d / StopDirection: %d / ServerX:%d / ServerY:%d / ClientX: %d / ClientY: %d ",
			player->sessionID, byDirection, player->sectorX, player->sectorY, shX, shY);

		CPacket* syncPacket = CPacket::Alloc();

		mpMySync(syncPacket, player->sessionID, player->sectorX, player->sectorY);

		// 나 포함 싱크 보정
		SendPacketAround(player, syncPacket, true);

		shX = player->sectorX;
		shY = player->sectorY;

		CPacket::Free(syncPacket);
	}

	// 공격은 정지한 채 진행
	player->dwAction = dfPACKET_MOVE_STOP;

	// 클라에서 온 패킷(클라의 좌표 데이터)을 서버가 그대로 믿음
	player->sectorX = shX;
	player->sectorY = shY;

	CPacket* attackPacket = CPacket::Alloc();

	// 현재 접속중인 모든 사용자에게 공격 패킷을 뿌리기 위한 패킷
	mpAttack2(attackPacket, sessionID, player->byDirection, player->sectorX, player->sectorY);

	// 주변 섹터에 존재하는 Player들에게 패킷 전송 (공격자 제외 전송)
	SendPacketAround(player, attackPacket);

	CPacket::Free(attackPacket);

	// player가 존재하는 섹터의 주변 9개 섹터 구하기
	st_SECTOR_AROUND sectorAround;
	GetSectorAround(player->CurSector.x, player->CurSector.y, &sectorAround);

	// 주변 섹터에서 공격 범위 내에 있는 캐릭터 검사
	for (int i = 0; i < sectorAround.iCount; i++)
	{
		auto iter = _sector[sectorAround.Around[i].y][sectorAround.Around[i].x].begin();

		// 얻어진 섹터의 리스트를 순회하면서 공격 범위 내에 있는 캐릭터 찾음
		for (; iter != _sector[sectorAround.Around[i].y][sectorAround.Around[i].x].end();)
		{
			Player* pExistPlayer = *iter;
			iter++;

			// 나 자신은 공격 범위 대상이 아니므로 스킵
			if (pExistPlayer == player)
				continue;

			// 뒤를 바라보고 공격하면 안됨 -> skip
			if (!isAttackBack(player, pExistPlayer))
				continue;

			// 공격 범위 내에 피해자 캐릭터가 있으면 캐릭터 데미지 패킷 송신
			if (abs(pExistPlayer->sectorX - shX) < dfATTACK2_RANGE_X &&
				abs(pExistPlayer->sectorY - shY) < dfATTACK2_RANGE_Y)
			{
				// 데미지
				pExistPlayer->cHP -= dfATTACK2_DAMAGE;

				// HP가 0 이하일 경우, 값 보정
				if (pExistPlayer->cHP <= 0)
					pExistPlayer->cHP = 0;

				CPacket* damagePacket = CPacket::Alloc();

				// 현재 접속중인 모든 사용자에게 데미지 패킷을 뿌림
				mpDamage(damagePacket, sessionID, pExistPlayer->sessionID, pExistPlayer->cHP);

				// 피해자 기준으로 섹터 전송
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

	// player 찾기
	Player* player = FindPlayer(sessionID);
	if (player == nullptr)
	{
		_log->logger(dfLOG_LEVEL_ERROR, __LINE__, L"Attack3 # %016llx Player Not Found!", sessionID);
		CRASH();
	}

	*packet >> byDirection >> shX >> shY;

	// 서버의 위치와 받은 패킷의 위치값이 너무 큰 차이가 날 경우, 싱크 패킷을 보내 좌표 보정
	if (abs(player->sectorX - shX) > dfERROR_RANGE || abs(player->sectorY - shY) > dfERROR_RANGE)
	{
		_log->logger(dfLOG_LEVEL_ERROR, __LINE__, L"# SYNC > Session ID: %d / StopDirection: %d / ServerX:%d / ServerY:%d / ClientX: %d / ClientY: %d ",
			player->sessionID, byDirection, player->sectorX, player->sectorY, shX, shY);

		CPacket* syncPacket = CPacket::Alloc();

		mpMySync(syncPacket, player->sessionID, player->sectorX, player->sectorY);

		// 나 포함 싱크 보정
		SendPacketAround(player, syncPacket, true);

		shX = player->sectorX;
		shY = player->sectorY;

		CPacket::Free(syncPacket);
	}

	// 공격은 정지한 채 진행
	player->dwAction = dfPACKET_MOVE_STOP;

	// 클라에서 온 패킷(클라의 좌표 데이터)을 서버가 그대로 믿음
	player->sectorX = shX;
	player->sectorY = shY;

	CPacket* attackPacket = CPacket::Alloc();

	// 현재 접속중인 모든 사용자에게 공격 패킷을 뿌리기 위한 패킷
	mpAttack3(attackPacket, sessionID, player->byDirection, player->sectorX, player->sectorY);

	// 주변 섹터에 존재하는 Player들에게 패킷 전송 (공격자 제외 전송)
	SendPacketAround(player, attackPacket);

	CPacket::Free(attackPacket);

	// player가 존재하는 섹터의 주변 9개 섹터 구하기
	st_SECTOR_AROUND sectorAround;
	GetSectorAround(player->CurSector.x, player->CurSector.y, &sectorAround);

	// 주변 섹터에서 공격 범위 내에 있는 캐릭터 검사
	for (int i = 0; i < sectorAround.iCount; i++)
	{
		auto iter = _sector[sectorAround.Around[i].y][sectorAround.Around[i].x].begin();

		// 얻어진 섹터의 리스트를 순회하면서 공격 범위 내에 있는 캐릭터 찾음
		for (; iter != _sector[sectorAround.Around[i].y][sectorAround.Around[i].x].end();)
		{
			Player* pExistPlayer = *iter;
			iter++;

			// 나 자신은 공격 범위 대상이 아니므로 스킵
			if (pExistPlayer == player)
				continue;

			// 뒤를 바라보고 공격하면 안됨 -> skip
			if (!isAttackBack(player, pExistPlayer))
				continue;

			// 공격 범위 내에 피해자 캐릭터가 있으면 캐릭터 데미지 패킷 송신
			if (abs(pExistPlayer->sectorX - shX) < dfATTACK3_RANGE_X &&
				abs(pExistPlayer->sectorY - shY) < dfATTACK3_RANGE_Y)
			{
				// 데미지
				pExistPlayer->cHP -= dfATTACK3_DAMAGE;

				// HP가 0 이하일 경우, 값 보정
				if (pExistPlayer->cHP <= 0)
					pExistPlayer->cHP = 0;

				CPacket* damagePacket = CPacket::Alloc();

				// 현재 접속중인 모든 사용자에게 데미지 패킷을 뿌림
				mpDamage(damagePacket, sessionID, pExistPlayer->sessionID, pExistPlayer->cHP);

				// 피해자 기준으로 섹터 전송
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

// 공격자가 반대편을 보고 공격하는지 판단
bool MMOServer::isAttackBack(Player* Attacker, Player* Damaged)
{
	// 피해자가 바라보는 방향이 왼쪽일 경우, 공격자는 더 오른쪽 위치에서 같은 방향을 봐야함
	// 피해자가 바라보는 방향이 왼쪽일 경우, 공격자는 더 왼쪽 위치에서 반대 방향을 봐야함 (서로 마주보는 상태)
	// 피해자가 바라보는 방향이 오른쪽일 경우, 공격자는 더 왼쪽 위치에서 같은 방향을 봐야함
	// 피해자가 바라보는 방향이 오른쪽일 경우, 공격자는 더 오른쪽 위치에서 반대 방향을 봐야함 (서로 마주보는 상태)
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

// 섹터에 변화가 생겼을 경우에만 실행
void MMOServer::CharacterSectorUpdatePacket(Player* player)
{
	st_SECTOR_AROUND RemoveSector, AddSector;

	RemoveSector.iCount = AddSector.iCount = 0;

	int iCnt;

	CPacket* delPacket = CPacket::Alloc();

	// 삭제할 섹터 & 추가할 섹터 구함
	GetUpdateSectorAround(player, &RemoveSector, &AddSector);

	// 1. RemoveSector에 캐릭터 삭제 패킷 보내기
	mpDelete(delPacket, player->sessionID);

	// 2. 지금 움직이는 유저에게, RemoveSector의 캐릭터들 삭제 패킷 보내기
	for (iCnt = 0; iCnt < RemoveSector.iCount; iCnt++)
	{
		// 특정섹터 한 공간에만 메시지를 전달하는 함수
		SendPacketSectorOne(RemoveSector.Around[iCnt].x, RemoveSector.Around[iCnt].y, delPacket, nullptr);

		// 얻어진 섹터를 돌면서 섹터리스트 접근
		auto iter = _sector[RemoveSector.Around[iCnt].y][RemoveSector.Around[iCnt].x].begin();

		// 해당 섹터마다 등록된 캐릭터들을 뽑아 삭제패킷 만들어 보냄
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

	// 3. AddSector에 캐릭터 생성 패킷 보내기
	mpCreateOther(myPacket, player->sessionID, player->byDirection, player->sectorX, player->sectorX, player->cHP);

	// 3-1. AddSector에 생성된 캐릭터 이동 패킷 보내기
	mpMoveStart(myMovePacket, player->sessionID, player->dwAction, player->sectorX, player->sectorY);

	for (iCnt = 0; iCnt < AddSector.iCount; iCnt++)
	{
		// 특정섹터 한 공간에만 메시지를 전달하는 함수
		SendPacketSectorOne(AddSector.Around[iCnt].x, AddSector.Around[iCnt].y, myPacket, nullptr);
		SendPacketSectorOne(AddSector.Around[iCnt].x, AddSector.Around[iCnt].y, myMovePacket, nullptr);

		// 4. 이동한 캐릭터에게 AddSector에 있는 캐릭터들 생성 패킷 보내기
		auto iter = _sector[AddSector.Around[iCnt].y][AddSector.Around[iCnt].x].begin();

		// 해당 섹터마다 등록된 캐릭터들을 뽑아 생성패킷 만들어 보냄
		for (; iter != _sector[AddSector.Around[iCnt].y][AddSector.Around[iCnt].x].end();)
		{
			Player* pExistPlayer = *iter;
			++iter;
			if (pExistPlayer == player) continue;
			CPacket* packetOther = CPacket::Alloc();

			// 내 주변 캐릭터
			mpCreateOther(packetOther, pExistPlayer->sessionID,	pExistPlayer->byDirection, pExistPlayer->sectorX, pExistPlayer->sectorY, pExistPlayer->cHP);

			// 나한테 주변 섹터의 캐릭터 정보 전송
			SendPacket(player->sessionID, packetOther);
			
			CPacket::Free(packetOther);

			// 새 AddSector의 캐릭터가 걷고 있었다면 이동 패킷 만들어 보냄
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
	// player가 존재하는 섹터의 주변 9개 섹터 구하기
	st_SECTOR_AROUND sectorAround;
	GetSectorAround(player->CurSector.x, player->CurSector.x, &sectorAround);

	// 본인 포함 전송 (현재 접속중인 주변 섹터 내의 모든 사용자에게 패킷을 뿌림)
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
	// 본인 제외 전송
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

// 특정 섹터 1개에 있는 클라이언트들에게 메시지 보내기
void MMOServer::SendPacketSectorOne(int iSectorX, int iSectorY, CPacket* packet, Player* pExcepPlayer)
{
	auto iter = _sector[iSectorY][iSectorX].begin();

	// 특정 섹터에 있는 캐릭터들에게 메시지 send
	for (; iter != _sector[iSectorY][iSectorX].end();)
	{
		Player* curPlayer = *iter;
		++iter;

		// 제외할 캐릭터 있으면 스킵
		if (pExcepPlayer != nullptr && curPlayer == pExcepPlayer)
			continue;

		// 각 캐릭터 세션에 send
		SendPacket(curPlayer->sessionID, packet);
	}
}


// --------------------------------------------------------------------------------
// Rander Update
// --------------------------------------------------------------------------------
bool MMOServer::UpdateSkip()
{
	// Update() 호출 타이밍 확인용
	static DWORD oldTick = timeGetTime();
	DWORD curTick = timeGetTime();

	// 복구해야할 프레임
	static DWORD recoveryFrame = 0;

	// FPS 출력 용도
	static DWORD frameCheckOldTick = timeGetTime();

	// 기존 프레임
	static DWORD updateFPS = 0;

	// 복구한 프레임
	static DWORD recoveryUpdateFPS = 0;

	// 1초마다 FPS 출력
	if (curTick - frameCheckOldTick >= 1000)
	{
		// 프레임은 기존 프레임 + 복구한 프레임
		frameCheckOldTick += 1000;

		// updateFPS가 목표 프레임(25fps)보다 낮다면 복구 해야할 프레임이 있다는 의미
		if (updateFPS < 25)
		{
			// 복구 해야할 프레임에 추가
			recoveryFrame += (25 - updateFPS);
		}

		// 1초 지났으므로 FPS 초기화
		updateFPS = 0;
		recoveryUpdateFPS = 0;
	}

	// 40ms 넘을 때마다 한번씩 Update 호출
	if (curTick - oldTick >= 40)
	{
		oldTick += 40;
		updateFPS++;
		return true;
	}

	// 복구 해야할 프레임이 있다면 바로 update 호출
	if (recoveryFrame > 0)
	{
		// 복구해야할 프레임 1 감소
		recoveryFrame--;

		// 복구한 프레임 1 증가
		recoveryUpdateFPS++;
		return true;
	}

	return false;
}

void MMOServer::Update()
{
	// 적절한 게임 업데이트 처리 타이밍 계산 (25 fps)
	// 25 프레임마다 업데이트
	if (!UpdateSkip())
		return;

	DWORD curTicks = timeGetTime();

	// 현재 존재하는 모든 플레이어들에 대해 업데이트 수행
	for (auto iter = _mapPlayer.begin(); iter != _mapPlayer.end();)
	{
		Player* player = iter->second;
		iter++;
		
		if (player->cHP == 0)
		{
			// 사망 처리
			DisconnectSession(player->sessionID);
			continue;
		}

		// 현재 동작에 따른 처리
		switch (player->dwAction)
		{
		case dfPACKET_MOVE_STOP:

			break;
		case dfPACKET_MOVE_DIR_LL:
		{
			// 방향에 따른 이동처리
			if (player->sectorX - dfSPEED_PLAYER_X > dfRANGE_MOVE_LEFT)
			{
				player->sectorX -= dfSPEED_PLAYER_X;
			}
			break;
		}
		case dfPACKET_MOVE_DIR_LU:
		{
			// 방향에 따른 이동처리
			// x가 왼쪽 화면 영역 넘어설 경우 y 좌표는 건들면 안됨
			if (player->sectorX - dfSPEED_PLAYER_X > dfRANGE_MOVE_LEFT && player->sectorY - dfSPEED_PLAYER_Y > dfRANGE_MOVE_TOP)
			{
				player->sectorX -= dfSPEED_PLAYER_X;
				player->sectorY -= dfSPEED_PLAYER_Y;
			}

			break;
		}
		case dfPACKET_MOVE_DIR_UU:
		{
			// 방향에 따른 이동처리
			if (player->sectorY - dfSPEED_PLAYER_X > dfRANGE_MOVE_TOP)
			{
				player->sectorY -= dfSPEED_PLAYER_Y;
			}
			break;
		}
		case dfPACKET_MOVE_DIR_RU:
		{
			// 방향에 따른 이동처리
			if (player->sectorX + dfSPEED_PLAYER_X < dfRANGE_MOVE_RIGHT && player->sectorY - dfSPEED_PLAYER_Y > dfRANGE_MOVE_TOP)
			{
				player->sectorX += dfSPEED_PLAYER_X;
				player->sectorY -= dfSPEED_PLAYER_Y;
			}
			break;
		}
		case dfPACKET_MOVE_DIR_RR:
		{
			// 방향에 따른 이동처리
			if (player->sectorX + dfSPEED_PLAYER_X < dfRANGE_MOVE_RIGHT)
			{
				player->sectorX += dfSPEED_PLAYER_X;
			}
			break;
		}
		case dfPACKET_MOVE_DIR_RD:
		{
			// 방향에 따른 이동처리
			if (player->sectorX + dfSPEED_PLAYER_X < dfRANGE_MOVE_RIGHT && player->sectorY + dfSPEED_PLAYER_Y < dfRANGE_MOVE_BOTTOM)
			{
				player->sectorX += dfSPEED_PLAYER_X;
				player->sectorY += dfSPEED_PLAYER_Y;
			}
			break;
		}
		case dfPACKET_MOVE_DIR_DD:
		{
			// 방향에 따른 이동처리
			if (player->sectorY + dfSPEED_PLAYER_Y < dfRANGE_MOVE_BOTTOM)
			{
				player->sectorY += dfSPEED_PLAYER_Y;
			}
			break;
		}
		case dfPACKET_MOVE_DIR_LD:
		{
			// 방향에 따른 이동처리
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

		// 이동하고 있을 경우
		if (player->dwAction != dfPACKET_MOVE_STOP)
		{
			// 섹터 업데이트
			if (SectorUpdateCharacter(player))
			{
				// 섹터가 변경된 경우는 클라에게 관련 정보 전송
				CharacterSectorUpdatePacket(player);
			}
		}
	}
}

// ------------------------------------------------------------------------
// Sector
// ------------------------------------------------------------------------
// 
// 특정 섹터 좌표 기준으로 주변 최대 9개의 섹터 좌표를 얻어옴
void MMOServer::GetSectorAround(int iSectorX, int iSectorY, st_SECTOR_AROUND* pSectorAround)
{
	// 현재 내 섹터에서 좌측상단부터 섹터 좌표를 얻기 위함
	iSectorX--;
	iSectorY--;

	pSectorAround->iCount = 0;

	for (int iY = 0; iY < 3; iY++)
	{
		// 가장자리에 위치해 있을 경우, 섹터 범위를 벗어나는 것들은 제외
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

// 캐릭터가 섹터를 이동 한 뒤, 영향권에서 빠진 섹터(삭제할 섹터),, 새로 추가된 섹터의 정보 구하는 함수
void MMOServer::GetUpdateSectorAround(Player* pCharacter, st_SECTOR_AROUND* pRemoveSector, st_SECTOR_AROUND* pAddSector)
{
	// 좌표 이동 전 섹터 구하기
	st_SECTOR_AROUND oldSector;
	GetSectorAround(pCharacter->OldSector.x, pCharacter->OldSector.y, &oldSector);

	// 현재 내 섹터 구하기
	st_SECTOR_AROUND CurSector;
	GetSectorAround(pCharacter->CurSector.x, pCharacter->CurSector.y, &CurSector);

	bool flag = false;

	pRemoveSector->iCount = 0;

	// oldSector에서 겹치는 섹터 제외한 부분이 removeSector
	for (int i = 0; i < oldSector.iCount; i++)
	{
		flag = false;

		for (int j = 0; j < CurSector.iCount; j++)
		{
			// 겹치는 부분은 삭제할 섹터가 아님
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

	// CurSector에서 겹치는 섹터 제외한 부분이 addSector
	for (int i = 0; i < CurSector.iCount; i++)
	{
		flag = false;

		for (int j = 0; j < oldSector.iCount; j++)
		{
			// 겹치는 부분은 추가할 섹터가 아님
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

// 캐릭터의 월드 좌표 x, y를 토대로 섹터의 좌표를 실제로 변경, 갱신하는 함수
bool MMOServer::SectorUpdateCharacter(Player* pCharacter)
{
	int curX;
	int curY;

	// 좌표 보정
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

	// 이전 섹터 좌표와 현재 섹터 좌표를 비교하여 변경됐는지 확인
	if (pCharacter->CurSector.x == sectorPos.x && pCharacter->CurSector.y == sectorPos.y)
		return false;

	pCharacter->OldSector = pCharacter->CurSector;

	auto iter = _sector[pCharacter->OldSector.y][pCharacter->OldSector.x].find(pCharacter);

	// 제거도 안했는데, set 안에 없다는 건 말이 안됨 - error
	if (iter == _sector[pCharacter->OldSector.y][pCharacter->OldSector.x].end())
	{
		return false;
	}

	// 이전 섹터에서 해당 플레이어 삭제
	_sector[pCharacter->OldSector.y][pCharacter->OldSector.x].erase(pCharacter);

	pCharacter->CurSector = sectorPos;

	// 현재 섹터에서 해당 플레이어 추가
	_sector[pCharacter->CurSector.y][pCharacter->CurSector.x].insert(pCharacter);

	return true;
}
