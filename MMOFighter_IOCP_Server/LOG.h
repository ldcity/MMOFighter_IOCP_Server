#ifndef __LOG__H__
#define __LOG__H__

#include <Windows.h>
#include <stdio.h>
#include <time.h>

#define dfLOG_LEVEL_DEBUG		0
#define dfLOG_LEVEL_ERROR		1
#define dfLOG_LEVEL_SYSTEM		2

// ���ϰ� �߻��ϴ� �����ڵ�
#define ERROR_10054		10054
#define ERROR_10058		10058
#define ERROR_10060		10060

class Log
{
private:
	FILE* fp;
	int g_iLogLevel;				// ��� ���� ����� �α� ����
	wchar_t g_szLogBuff[1024];		// �α� ����� �ʿ��� �ӽ� ����
	CRITICAL_SECTION cs;

public:
	Log() {};

	Log(const WCHAR* szFileName)
	{
		fp = nullptr;
		g_iLogLevel = -1;
		ZeroMemory(g_szLogBuff, sizeof(wchar_t));

		time_t now;
		struct tm timeinfo;
		time(&now);
		localtime_s(&timeinfo, &now);

		// �α� ���� ��� ����
		wchar_t logFolderPath[MAX_PATH];
		GetCurrentDirectoryW(MAX_PATH, logFolderPath);
		wcscat_s(logFolderPath, L"\\LOG");

		// �α� ������ ���ٸ� ����
		if (!CreateDirectoryW(logFolderPath, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
		{
			// ���� ������ �����ϸ� ���� ó��
			wprintf(L"Failed to create LOG folder. Error code: %d\n", GetLastError());
			return;
		}

		// �α� ���� ������ ���� �ð� + szFileName���� ����
		wchar_t logFileName[MAX_PATH];
		wcsftime(logFileName, sizeof(logFileName) / sizeof(wchar_t), L"Log_%Y%m%d%H%M%S_", &timeinfo);
		wcscat_s(logFileName, sizeof(logFileName) / sizeof(wchar_t), szFileName);
		wcscat_s(logFileName, sizeof(logFileName) / sizeof(wchar_t), L".txt");

		// �α� ���� ��ü ��� ����
		wchar_t fullPath[MAX_PATH];
		wcscpy_s(fullPath, logFolderPath);
		wcscat_s(fullPath, L"\\");
		wcscat_s(fullPath, logFileName);

		_wfopen_s(&fp, fullPath, L"w");
		if (fp == nullptr)
		{
			// ���� ������ �����ϸ� ���� ó��
			wprintf(L"Failed to open LOG File. Error code: %d\n", GetLastError());
			return;
		}

		InitializeCriticalSection(&cs);
	}

	void logger(int iLogLevel, int lines, const wchar_t* format, ...)
	{
		EnterCriticalSection(&cs);
		if (g_iLogLevel <= iLogLevel)
		{
			va_list args;
			va_start(args, format);
			vswprintf_s(g_szLogBuff, format, args);
			va_end(args);

			if (iLogLevel == dfLOG_LEVEL_DEBUG)
				fwprintf_s(fp, L"[DEBUG : %d] : %s\n", lines, g_szLogBuff);
			else if (iLogLevel == dfLOG_LEVEL_ERROR)
				fwprintf_s(fp, L"[ERROR : %d] : %s\n", lines, g_szLogBuff);
			else if (iLogLevel == dfLOG_LEVEL_SYSTEM)
				fwprintf_s(fp, L"[SYSTEM : %d] : %s\n", lines, g_szLogBuff);

			fflush(fp);
		}
		LeaveCriticalSection(&cs);
	}

	~Log()
	{
		if (fp != nullptr)
			fclose(fp);

		DeleteCriticalSection(&cs);
	}
};

#endif