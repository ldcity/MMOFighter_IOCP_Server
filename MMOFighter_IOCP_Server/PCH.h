#ifndef __PCH__
#define __PCH__

#include <WinSock2.h>
#include <WS2tcpip.h>
#include <windows.h>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "ws2_32.lib")

#include <process.h>

#include <unordered_map>
#include <unordered_set>
#include <set>
#include <vector>

#include <iostream>

#include "TLSFreeList.h"
#include "LockFreeQueue.h"
#include "LockFreeStack.h"

#include "Define.h"
#include "Exception.h"
#include "SerializingBuffer.h"
#include "RingBuffer.h"
#include "Session.h"
#include "Protocol.h"
#include "Sector.h"
#include "Player.h"

#include "LOG.h"
#include "CrashDump.h"
#include "TextParser.h"
#include "PerformanceMonitor.h"

#endif // __PCH__
