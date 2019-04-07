#include <conio.h>
#include <Windows.h>
#include <process.h>
#include <cstdio>
#pragma comment(lib,"winmm.lib")
#include "CLFMemoryPool_TLS.h"
#include "CProfiler.h"

#ifdef __PROFILER__
#define BEGIN(szName) Profiler.Begin(szName)
#define END(szName) Profiler.End(szName)
#define PRINT() Profiler.PrintData()
#else
#define BEGIN(szName) {}
#define END(szName) {}
#define PRINT() {}
#endif

#define df_THREAD_CNT 5
#define df_ALLOC_CNT 10000

struct st_TEST_DATA
{
	volatile LONG64	lData;
	volatile LONG64	lCount;
};

LONG64 g_lTlsAllocTps, g_lTlsFreeTps;
LONG64 g_lAllocTps, g_lFreeTps;

bool g_bShutdown, g_bStop;

mylib::CLFMemoryPool_TLS<st_TEST_DATA> * g_pMemPoolTLS;
mylib::CLFMemoryPool<st_TEST_DATA> * g_pMemPool;

bool Control();
void PrintState();
unsigned int __stdcall TLSWorkerThread(LPVOID pParam);
unsigned int __stdcall WorkerThread(LPVOID pParam);

void main()
{
	timeBeginPeriod(1);

	g_pMemPoolTLS = new mylib::CLFMemoryPool_TLS<st_TEST_DATA>();
	g_pMemPool = new mylib::CLFMemoryPool<st_TEST_DATA>();

	HANDLE hTLSWorkerThread[df_THREAD_CNT];
	for (int iCnt = 0; iCnt < df_THREAD_CNT; ++iCnt)
		hTLSWorkerThread[iCnt] = (HANDLE)_beginthreadex(NULL, 0, TLSWorkerThread, NULL, FALSE, NULL);
	
	HANDLE hWorkerThread[df_THREAD_CNT];
	for (int iCnt = 0; iCnt < df_THREAD_CNT; ++iCnt)
		hWorkerThread[iCnt] = (HANDLE)_beginthreadex(NULL, 0, WorkerThread, NULL, FALSE, NULL);

	while (Control())
	{
		PrintState();
		Sleep(1);
	}

	g_bShutdown = true;

	WaitForMultipleObjects(df_THREAD_CNT, hTLSWorkerThread, TRUE, INFINITE);
	for (int iCnt = 0; iCnt < df_THREAD_CNT; ++iCnt)
		CloseHandle(hTLSWorkerThread[iCnt]);

	WaitForMultipleObjects(df_THREAD_CNT, hWorkerThread, TRUE, INFINITE);
	for (int iCnt = 0; iCnt < df_THREAD_CNT; ++iCnt)
		CloseHandle(hWorkerThread[iCnt]);
	
	timeEndPeriod(1);
}

bool Control()
{
	static bool bControl = false;
	WCHAR chKey;
	if (_kbhit())
	{
		chKey = _getwch();
		switch (chKey)
		{
		case L'u':
		case L'U':
			if (!bControl)
			{
				bControl = true;
				wprintf(L"[ Control Mode ] \n");
				wprintf(L"Press  L	- Key Lock \n");
				wprintf(L"Press  S	- WorkerThread Stop/Start \n");
				wprintf(L"Press  Q	- Quit \n");
			}
			break;
		case L'l':
		case L'L':
			if (bControl)
			{
				bControl = false;
				wprintf(L"[ Control Mode ] \n");
				wprintf(L"Press  U	- Key Unlock \n");
				wprintf(L"Press  S	- WorkerThread Stop/Start \n");
				wprintf(L"Press  Q	- Quit \n");
			}
			break;
		case L's':
		case L'S':
			if (bControl)
			{
				if (!g_bStop)
					g_bStop = true;
				else if (g_bStop)
					g_bStop = false;
			}
			break;
		case L'q':
		case L'Q':
			if (bControl)
				return false;
			break;
		}
	}

	return true;
}

void PrintState()
{
	static DWORD dwStartTick = timeGetTime();
	static DWORD dwSystemTick = timeGetTime();
	DWORD dwCurrentTick = timeGetTime();

	// 서버에서 반드시 검사해야하는 최소한의 항목
	if (dwCurrentTick - dwSystemTick >= 500)
	{
		system("cls");
		wprintf(L" Elapsed Time : %d\n", (dwCurrentTick - dwStartTick)/1000);
		wprintf(L"===========================================\n");
		wprintf(L" LFMemoryPool_TLS\n\n");

		wprintf(L" - Alloc TPS		: %lld \n", g_lTlsAllocTps);
		wprintf(L" - Free TPS		: %lld \n", g_lTlsFreeTps);
		wprintf(L" - MemPool Use Size	: %d \n", g_pMemPoolTLS->GetUseSize());
		wprintf(L" - MemPool Alloc Size	: %d \t(Chunk Unit) \n\n", g_pMemPoolTLS->GetAllocSize());


		wprintf(L"===========================================\n");
		wprintf(L" LFMemoryPool\n\n");

		wprintf(L" - Alloc TPS		: %lld \n", g_lAllocTps);
		wprintf(L" - Free TPS		: %lld \n", g_lFreeTps);
		wprintf(L" - MemPool Use Size	: %d \n", g_pMemPool->GetUseSize());
		wprintf(L" - MemPool Alloc Size	: %d \n", g_pMemPool->GetAllocSize());

		dwSystemTick = dwCurrentTick;
		g_lTlsAllocTps = g_lTlsFreeTps = g_lAllocTps = g_lFreeTps = 0;
	}
}

// 여러개의 스레드에서 일정수량의 Alloc 과 Free 를 반복적으로 함
// 모든 데이터는 0x0000000055555555 으로 초기화 되어 있음.

// 사전에 100,000 개의 st_TEST_DATA  데이터를 메모리풀에 넣어둠. 
//  모든 데이터는 Data - 0x0000000055555555 / Cound - 0 초기화.

// 각각의 스레드는 아래의 행동을 루프 (스레드 10개)

// 0. Alloc (10000개)
// 1. 0x0000000055555555 이 맞는지 확인.
// 2. Interlocked + 1 (Data + 1 / Count + 1)
// 3. 약간대기
// 4. 여전히 0x0000000055555556 이 맞는지 (Count == 1) 확인.
// 5. Interlocked - 1 (Data - 1 / Count - 1)
// 6. 약간대기
// 7. 0x0000000055555555 이 맞는지 (Count == 0) 확인.
// 8. Free

unsigned int __stdcall TLSWorkerThread(LPVOID pParam)
{
	int iCnt;
	st_TEST_DATA *pData[df_ALLOC_CNT];

	while (!g_bShutdown)
	{
		if (g_bStop)
		{
			Sleep(1);
			continue;
		}

		// 0. Alloc (10000개)
		for (iCnt = 0; iCnt < df_ALLOC_CNT; ++iCnt)
		{
			BEGIN(L"TLSAlloc");
			pData[iCnt] = g_pMemPoolTLS->Alloc();
			END(L"TLSAlloc");
			pData[iCnt]->lCount = 0;
			pData[iCnt]->lData = 0x0000000055555555;
			InterlockedIncrement64(&g_lTlsAllocTps);
		}

		// 1. 0x0000000055555555 이 맞는지 확인.
		for (iCnt = 0; iCnt < df_ALLOC_CNT; ++iCnt)
		{
			if (pData[iCnt]->lData != 0x0000000055555555)
			{
				int *p = nullptr;
				*p = 0;
			}
		}

		// 2. Interlocked + 1 (Data + 1 / Count + 1)
		for (iCnt = 0; iCnt < df_ALLOC_CNT; ++iCnt)
		{
			InterlockedIncrement64(&pData[iCnt]->lCount);
			InterlockedIncrement64(&pData[iCnt]->lData);
		}

		// 3. 약간대기
		Sleep(1);

		// 4. 여전히 0x0000000055555556 이 맞는지 (Count == 1) 확인.
		for (iCnt = 0; iCnt < df_ALLOC_CNT; ++iCnt)
		{
			if (pData[iCnt]->lData != 0x0000000055555556 || pData[iCnt]->lCount != 1)
			{
				int *p = nullptr;
				*p = 0;
			}
		}

		// 5. Interlocked - 1 (Data - 1 / Count - 1)
		for (iCnt = 0; iCnt < df_ALLOC_CNT; ++iCnt)
		{
			InterlockedDecrement64(&pData[iCnt]->lCount);
			InterlockedDecrement64(&pData[iCnt]->lData);
		}

		// 6. 약간대기
		Sleep(1);

		// 7. 0x0000000055555555 이 맞는지 (Count == 0) 확인.
		for (iCnt = 0; iCnt < df_ALLOC_CNT; ++iCnt)
		{
			if (pData[iCnt]->lData != 0x0000000055555555 || pData[iCnt]->lCount != 0)
			{
				int *p = nullptr;
				*p = 0;
			}
		}

		// 8. Free
		for (iCnt = 0; iCnt < df_ALLOC_CNT; ++iCnt)
		{
			BEGIN(L"TLSFree");
			g_pMemPoolTLS->Free(pData[iCnt]);
			END(L"TLSFree");
			InterlockedIncrement64(&g_lTlsFreeTps);
		}
	}
	return 0;
}


unsigned int __stdcall WorkerThread(LPVOID pParam)
{
	int iCnt;
	st_TEST_DATA *pData[df_ALLOC_CNT];

	while (!g_bShutdown)
	{
		if (g_bStop)
		{
			Sleep(1);
			continue;
		}

		// 0. Alloc (10000개)
		for (iCnt = 0; iCnt < df_ALLOC_CNT; ++iCnt)
		{
			BEGIN(L"Alloc");
			pData[iCnt] = g_pMemPool->Alloc();
			END(L"Alloc");
			pData[iCnt]->lCount = 0;
			pData[iCnt]->lData = 0x0000000055555555;
			InterlockedIncrement64(&g_lAllocTps);
		}

		// 1. 0x0000000055555555 이 맞는지 확인.
		for (iCnt = 0; iCnt < df_ALLOC_CNT; ++iCnt)
		{
			if (pData[iCnt]->lData != 0x0000000055555555)
			{
				int *p = nullptr;
				*p = 0;
			}
		}

		// 2. Interlocked + 1 (Data + 1 / Count + 1)
		for (iCnt = 0; iCnt < df_ALLOC_CNT; ++iCnt)
		{
			InterlockedIncrement64(&pData[iCnt]->lCount);
			InterlockedIncrement64(&pData[iCnt]->lData);
		}

		// 3. 약간대기
		Sleep(1);

		// 4. 여전히 0x0000000055555556 이 맞는지 (Count == 1) 확인.
		for (iCnt = 0; iCnt < df_ALLOC_CNT; ++iCnt)
		{
			if (pData[iCnt]->lData != 0x0000000055555556 || pData[iCnt]->lCount != 1)
			{
				int *p = nullptr;
				*p = 0;
			}
		}

		// 5. Interlocked - 1 (Data - 1 / Count - 1)
		for (iCnt = 0; iCnt < df_ALLOC_CNT; ++iCnt)
		{
			InterlockedDecrement64(&pData[iCnt]->lCount);
			InterlockedDecrement64(&pData[iCnt]->lData);
		}

		// 6. 약간대기
		Sleep(1);

		// 7. 0x0000000055555555 이 맞는지 (Count == 0) 확인.
		for (iCnt = 0; iCnt < df_ALLOC_CNT; ++iCnt)
		{
			if (pData[iCnt]->lData != 0x0000000055555555 || pData[iCnt]->lCount != 0)
			{
				int *p = nullptr;
				*p = 0;
			}
		}

		// 8. Free
		for (iCnt = 0; iCnt < df_ALLOC_CNT; ++iCnt)
		{
			BEGIN(L"Free");
			g_pMemPool->Free(pData[iCnt]);
			END(L"Free");
			InterlockedIncrement64(&g_lFreeTps);
		}
	}
	return 0;
}