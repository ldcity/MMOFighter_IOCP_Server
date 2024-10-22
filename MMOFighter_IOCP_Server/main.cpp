#include "PCH.h"
#include "MMOServer.h"

lib::CrashDump crashDump;

MMOServer mmoServer;

int main()
{
	mmoServer.MMOServerStart();

	return 0;
}