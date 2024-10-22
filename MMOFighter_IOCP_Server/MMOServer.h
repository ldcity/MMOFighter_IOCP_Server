#ifndef __MMOSERVER__CLASS__
#define __MMOSERVER__CLASS__

#include "PCH.h"
#include "NetServer.h"

class MMOServer : public NetServer
{
public:
	MMOServer();
	~MMOServer();

	enum JobType
	{
		NEW_CONNECT,	// �� ����
		DISCONNECT,		// ���� ����
		MSG_PACKET,		// ���ŵ� ��Ŷ ó��
		TIMEOUT			// Ÿ�Ӿƿ�
	};

	bool MMOServerStart();
	bool MMOServerStop();

	bool OnConnectionRequest(const wchar_t* IP, unsigned short PORT);
	void OnClientJoin(uint32_t sessionID);
	void OnClientLeave(uint32_t sessionID);
	void OnRecv(uint32_t sessionID, CPacket* packet);
	void OnError(int errorCode, const wchar_t* msg);
	void OnTimeout(uint32_t sessionID);

	//--------------------------------------------------------------------------------------
	// player ���� �Լ�
	//--------------------------------------------------------------------------------------
	Player* FindPlayer(uint32_t sessionID)							// player �˻�
	{
		Player* player = nullptr;

		auto iter = _mapPlayer.find(sessionID);

		if (iter == _mapPlayer.end())
		{
			return nullptr;
		}

		return iter->second;
	}

	bool CreatePlayer(uint32_t sessionID);									// player ����
	bool DeletePlayer(uint32_t sessionID);

	//--------------------------------------------------------------------------------------
	// Make Packet
	//--------------------------------------------------------------------------------------
	void mpCreateMe(CPacket* packet, unsigned long ID, unsigned char Direction, short x, short y, unsigned char HP)
	{
		unsigned char type = dfPACKET_SC_CREATE_MY_CHARACTER;
		*packet << type << ID << Direction << x << y << HP;
	}

	void mpCreateOther(CPacket* packet, unsigned long ID, unsigned char Direction, short x, short y, unsigned char HP)
	{
		unsigned char type = dfPACKET_SC_CREATE_OTHER_CHARACTER;
		*packet << type << ID << Direction << x << y << HP;
	}

	void mpDelete(CPacket* packet, unsigned long ID)
	{
		unsigned char type = dfPACKET_SC_DELETE_CHARACTER;
		
		*packet << type << ID;
	}

	void mpMoveStart(CPacket* packet, unsigned long ID, unsigned char dwAction, short x, short y)
	{
		unsigned char type = dfPACKET_SC_MOVE_START;

		*packet << type << ID << dwAction << x << y;
	}

	void mpMoveStop(CPacket* packet, unsigned long ID, unsigned char Direction, short x, short y)
	{
		unsigned char type = dfPACKET_SC_MOVE_STOP;

		*packet << type << ID << Direction << x << y;
	}

	void mpAttack1(CPacket* packet, unsigned long AttackID, unsigned char Direction, short x, short y)
	{
		unsigned char type = dfPACKET_SC_ATTACK1;

		*packet << type << AttackID << Direction << x << y;
	}

	void mpAttack2(CPacket* packet, unsigned long AttackID, unsigned char Direction, short x, short y)
	{
		unsigned char type = dfPACKET_SC_ATTACK2;

		*packet << type << AttackID << Direction << x << y;
	}

	void mpAttack3(CPacket* packet, unsigned long AttackID, unsigned char Direction, short x, short y)
	{
		unsigned char type = dfPACKET_SC_ATTACK3;

		*packet << type << AttackID << Direction << x << y;
	}

	void mpDamage(CPacket* packet, unsigned long AttackID, unsigned long DamageID, unsigned char DamageHP)
	{
		unsigned char type = dfPACKET_SC_DAMAGE;

		*packet << type << AttackID << DamageID << DamageHP;
	}

	void mpMySync(CPacket* packet, unsigned long dwSessionID, short shX, short shY)
	{
		unsigned char type = dfPACKET_SC_SYNC;

		*packet << type << dwSessionID << shX << shY;
	}

	void mpEcho(CPacket* packet, unsigned long echo)
	{
		unsigned char type = dfPACKET_SC_ECHO;
	
		*packet << type << echo;
	}

	void CharacterSectorUpdatePacket(Player* pCharacter);

	//--------------------------------------------------------------------------------------
	// Packet Proc
	//--------------------------------------------------------------------------------------
	bool PacketProc(uint32_t sessionID, CPacket* packet);
	bool NetPacketProc_MoveStart(uint32_t sessionID, CPacket* packet);
	bool NetPacketProc_MoveStop(uint32_t sessionID, CPacket* packet);
	bool NetPacketProc_Attack1(uint32_t sessionID, CPacket* packet);
	bool NetPacketProc_Attack2(uint32_t sessionID, CPacket* packet);
	bool NetPacketProc_Attack3(uint32_t sessionID, CPacket* packet);
	bool NetPacketProc_Echo(uint32_t sessionID, CPacket* packet);

	// �ֺ� 9�� ���Ϳ� �ִ� Ŭ���̾�Ʈ�鿡�� �޽��� ������
	void SendPacketAround(Player* player, CPacket* packet, bool bSendMe = false);

	// Ư�� ���� 1���� �ִ� Ŭ���̾�Ʈ�鿡�� �޽��� ������
	void SendPacketSectorOne(int iSectorX, int iSectorY, CPacket* packet, Player* pExcepPlayer);

	// �ڿ��� �����ϴ��� �Ǵ�
	bool isAttackBack(Player* Attacker, Player* Damaged);

public:
	bool UpdateSkip();
	void Update();

public:
	friend unsigned __stdcall MoniteringThread(void* param);				// ����͸� ������
	friend unsigned __stdcall JobWorkerThread(PVOID param);					// ���� Job ó�� ������

	bool MoniterThread_Serv();
	bool JobWorkerThread_Serv();

public:
	// Ư�� ���� ��ǥ �������� �ֺ� �ִ� 9���� ���� ��ǥ�� ����
	void GetSectorAround(int iSectorX, int iSectorY, st_SECTOR_AROUND* pSectorAround);

	// ĳ���Ͱ� ���͸� �̵� �� ��, ����ǿ��� ���� ����(������ ����),, ���� �߰��� ������ ���� ���ϴ� �Լ�
	void GetUpdateSectorAround(Player* pCharacter, st_SECTOR_AROUND* pRemoveSector, st_SECTOR_AROUND* pAddSector);

	// ĳ������ ���� ��ǥ x, y�� ���� ������ ��ǥ�� ������ ����, �����ϴ� �Լ�
	bool SectorUpdateCharacter(Player* pCharacter);

public:
		std::set<Player*> _sector[dfSECTOR_Y_MAX][dfSECTOR_X_MAX];		// �� ���Ϳ� �����ϴ� Player ��ü

private:
	Log* _log;
	PerformanceMonitor _performMoniter;

	wchar_t _ip[16];
	unsigned short _port;

	std::wstring _wIP;
	std::string _IP;

private:
	// Job ����ü
	struct Job
	{
		// Session ID
		uint32_t sessionID;

		// Job Type
		WORD type;

		// ��Ŷ ������
		CPacket* packet;
	};

private:
	int _userMAXCnt;									// �ִ� player ��
	int _timeout;

	HANDLE _moniteringThread;							// Monitering Thread
	HANDLE _controlThread;

	HANDLE _moniterEvent;								// Monitering Event
	HANDLE _runEvent;									// Thread Start Event

	HANDLE _jobHandle;
	HANDLE _jobEvent;

	TLSObjectPool<Job> _jobPool = TLSObjectPool<Job>(300);
	LockFreeQueue<Job*> _jobQ = LockFreeQueue<Job*>(500);

	TLSObjectPool<Player> _playerPool = TLSObjectPool<Player>(190);
	
	std::unordered_map<uint32_t, Player*> _mapPlayer;							// ��ü Player ��ü

private:
	__int64 _jobUpdateTPS;
	__int64 _jobUpdateCnt;

	__int64 _playerCnt;
	__int64 _playerTPS;

private:
	bool _startFlag;
};


#endif