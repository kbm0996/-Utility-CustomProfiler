#include "CProfiler.h"

/*-------------------------------------------------------------------
// QueryPerformanceCounter / QueryPerformanceFrequency 사용 예 //
__int64 GetQPCTick()
{
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return static_cast<__int64>(li.QuadPart);
}
void main()
{
	LARGE_INTEGER li;
	QueryPerformanceFrequency(&li);

	__int64 StartTick = GetQPCTick();   // 시작 count를 구해놓고...
	TestFunc();
	__int64 EndTick = GetQPCTick();     // 끝 count를 구한다.

	cout << "Consumed time : " << (EndTick - StartTick) / (li.QuadPart) << " s" << endl;
}
-------------------------------------------------------------------*/

CProfiler Profiler;

CProfiler::CProfiler()
{
	_wsetlocale(LC_ALL, L"korean");

	QueryPerformanceFrequency(&_IFrequency);	// return :	머신이 QueyPerformanceCounter 지원시 TRUE, 아니면 FALSE.
	QueryPerformanceCounter(&_lCurCnt);			// return :	성공시 TRUE, 아니면 FALSE.
	
	for (int iCnt = 0; iCnt < en_MAX_THREAD; ++iCnt)
	{
		_stTLS[iCnt].dwThreadID = -1;
		for (int iCnt2 = 0; iCnt2 < en_MAX_DATA; ++iCnt2)
		{
			_stTLS[iCnt].stData[iCnt2].bUse = false;
			for (int iCnt3 = 0; iCnt3 < 2; ++iCnt3)
			{
				_stTLS[iCnt].stData[iCnt2].dMax[iCnt3] = 0;
				_stTLS[iCnt].stData[iCnt2].dMin[iCnt3] = 0x7fffffff;	// unsigned int의 최댓값 4294967295 0x7ffffff 삽입
			}
			_stTLS[iCnt].stData[iCnt2].dTotalTime = 0;
			_stTLS[iCnt].stData[iCnt2].iCall = 0;
		}
	}
	_lTLSCnt = -1;

	/////////////////////////////////////////////////////////////////////////////////////////
	//  TLS - 스레드별 독립적인 저장소
	//
	// OS 차원에서 지원해주는 기능. TLS는 유한(TLS_MINIMUM_AVAILABLE = 64)하므로 아껴써야함
	/////////////////////////////////////////////////////////////////////////////////////////
	//  TlsAlloc() - 현재 사용 중인 TLS가 무엇인지 알 수 없으므로 TLS관리자로부터(관리자는 내부 bitflag로 식별)얻어와야함 
	_dwTlsIndex = TlsAlloc();	
	if (_dwTlsIndex == TLS_OUT_OF_INDEXES)
	{
		// TLS_OUT_OF_INDEXES : TLS를 얻지 못함 
		// 대부분 가용할 수 있는 TLS가 없는 경우. 다른데서 사용되는 TLS를 줄이는 것 외에 방법이 없음
		LOG(L"PROFILER", LOG_ERROR, L"TLS OUT OF INDEXES");
		CRASH(); 
		exit(0);
	}
}

CProfiler::~CProfiler()
{
	PrintData();
	TlsFree(_dwTlsIndex); // TLS 해제
}

void CProfiler::Begin(WCHAR * szName)
{
	// TLS로부터 데이터 얻기
	st_PROFILE_TLS *pInfo = (st_PROFILE_TLS*)TlsGetValue(_dwTlsIndex);
	if (pInfo == NULL)	// 첫 기록일 경우 (TlsAlloc()은 성공시 NULL로 초기화됨)
	{
		InterlockedIncrement(&_lTLSCnt);
		
		pInfo = &_stTLS[_lTLSCnt];
		pInfo->dwThreadID = GetCurrentThreadId();

		TlsSetValue(_dwTlsIndex, pInfo); // TLS에 데이터 저장
	}

	// Begin Time 저장
	for (int iCnt = 0; iCnt < en_MAX_DATA; ++iCnt)
	{
		if (!pInfo->stData[iCnt].bUse)
		{
			pInfo->stData[iCnt].bUse = true;

			wcscpy_s(pInfo->stData[iCnt].szName, szName);
			QueryPerformanceCounter(&pInfo->stData[iCnt].lBeginTime);
			break;
		}

		if (wcscmp(pInfo->stData[iCnt].szName, szName) == 0)
		{
			QueryPerformanceCounter(&pInfo->stData[iCnt].lBeginTime);
			break;
		}
	}
}

void CProfiler::End(WCHAR * szName)
{
	// TLS로부터 데이터 얻기
	st_PROFILE_TLS *pInfo = (st_PROFILE_TLS*)TlsGetValue(_dwTlsIndex);
	if (pInfo == NULL)	// 첫 기록일 경우 (TlsAlloc()은 성공시 NULL로 초기화됨)
	{
		LOG(L"PROFILER", LOG_ERROR, L"End() # Not Match with Begin()");
		CRASH();
	}

	// End Time 저장
	LARGE_INTEGER lEndTime;
	QueryPerformanceCounter(&lEndTime);
	for (int iCnt = 0; iCnt < en_MAX_DATA; ++iCnt)
	{
		if (wcscmp(pInfo->stData[iCnt].szName, szName) == 0)
		{
			LONGLONG dTime = (lEndTime.QuadPart - pInfo->stData[iCnt].lBeginTime.QuadPart);
			pInfo->stData[iCnt].lBeginTime.QuadPart = 0;
			pInfo->stData[iCnt].dTotalTime += dTime;	// 누적 시간

			// 최대, 최소 시간
			if (pInfo->stData[iCnt].dMin[0] > pInfo->stData[iCnt].dMin[1] && 
				pInfo->stData[iCnt].dMax[0] < pInfo->stData[iCnt].dMax[1])
			{
				pInfo->stData[iCnt].dMin[0] = min(pInfo->stData[iCnt].dMin[0], dTime);
				pInfo->stData[iCnt].dMax[0] = max(pInfo->stData[iCnt].dMax[0], dTime);
			}
			else
			{
				pInfo->stData[iCnt].dMin[1] = min(pInfo->stData[iCnt].dMin[1], dTime);
				pInfo->stData[iCnt].dMax[1] = max(pInfo->stData[iCnt].dMax[1], dTime);
			}

			pInfo->stData[iCnt].iCall++;	// 호출 Cnt
			break;
		}
	}
}

void CProfiler::PrintData()
{
	FILE * pfProfile;
	_wfopen_s(&pfProfile, L"PROFILER.txt", L"w");

	fputws(L"      ThreadID      |        Name        |        Average     |          Min       |         Max        |   Call   \r\n", pfProfile);
	fputws(L"-------------------------------------------------------------------------------------------------------------------\r\n", pfProfile);
	WCHAR szString[128];
	for (int iCnt = 0; iCnt < en_MAX_THREAD; ++iCnt)
	{
		if (_stTLS[iCnt].dwThreadID != -1)
		{
			for (int iCnt2 = 0; iCnt2 < en_MAX_DATA; ++iCnt2)
			{
				if (_stTLS[iCnt].stData[iCnt2].bUse)
				{
					swprintf_s(szString, 128, L"%20d|%20s|%18f㎲|%18f㎲|%18f㎲|%10lld   \r\n",
						_stTLS[iCnt].dwThreadID,
						_stTLS[iCnt].stData[iCnt2].szName,
						(_stTLS[iCnt].stData[iCnt2].dTotalTime - _stTLS[iCnt].stData[iCnt2].dMin[0] - _stTLS[iCnt].stData[iCnt2].dMax[0]) * 1000000 / (double)(_IFrequency.QuadPart) / (_stTLS[iCnt].stData[iCnt2].iCall - 2),
						_stTLS[iCnt].stData[iCnt2].dMin[0] * 1000000 / (_IFrequency.QuadPart),
						_stTLS[iCnt].stData[iCnt2].dMax[0] * 1000000 / (_IFrequency.QuadPart),
						_stTLS[iCnt].stData[iCnt2].iCall);
					fputws(szString, pfProfile);
				}
				else
					break;
			}
			fputws(L"-------------------------------------------------------------------------------------------------------------------\r\n", pfProfile);
		}
		else
			break;
	}
	fclose(pfProfile);
}