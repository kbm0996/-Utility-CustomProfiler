#include "CProfiler.h"

/*-------------------------------------------------------------------
// QueryPerformanceCounter / QueryPerformanceFrequency ��� �� //
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

	__int64 StartTick = GetQPCTick();   // ���� count�� ���س���...
	TestFunc();
	__int64 EndTick = GetQPCTick();     // �� count�� ���Ѵ�.

	cout << "Consumed time : " << (EndTick - StartTick) / (li.QuadPart) << " s" << endl;
}
-------------------------------------------------------------------*/

CProfiler Profiler;

CProfiler::CProfiler()
{
	_wsetlocale(LC_ALL, L"korean");

	QueryPerformanceFrequency(&_IFrequency);	// return :	�ӽ��� QueyPerformanceCounter ������ TRUE, �ƴϸ� FALSE.
	QueryPerformanceCounter(&_lCurCnt);			// return :	������ TRUE, �ƴϸ� FALSE.
	
	for (int iCnt = 0; iCnt < en_MAX_THREAD; ++iCnt)
	{
		_stTLS[iCnt].dwThreadID = -1;
		for (int iCnt2 = 0; iCnt2 < en_MAX_DATA; ++iCnt2)
		{
			_stTLS[iCnt].stData[iCnt2].bUse = false;
			for (int iCnt3 = 0; iCnt3 < 2; ++iCnt3)
			{
				_stTLS[iCnt].stData[iCnt2].dMax[iCnt3] = 0;
				_stTLS[iCnt].stData[iCnt2].dMin[iCnt3] = 0x7fffffff;	// unsigned int�� �ִ� 4294967295 0x7ffffff ����
			}
			_stTLS[iCnt].stData[iCnt2].dTotalTime = 0;
			_stTLS[iCnt].stData[iCnt2].iCall = 0;
		}
	}
	_lTLSCnt = -1;

	/////////////////////////////////////////////////////////////////////////////////////////
	//  TLS - �����庰 �������� �����
	//
	// OS �������� �������ִ� ���. TLS�� ����(TLS_MINIMUM_AVAILABLE = 64)�ϹǷ� �Ʋ������
	/////////////////////////////////////////////////////////////////////////////////////////
	//  TlsAlloc() - ���� ��� ���� TLS�� �������� �� �� �����Ƿ� TLS�����ڷκ���(�����ڴ� ���� bitflag�� �ĺ�)���;��� 
	_dwTlsIndex = TlsAlloc();	
	if (_dwTlsIndex == TLS_OUT_OF_INDEXES)
	{
		// TLS_OUT_OF_INDEXES : TLS�� ���� ���� 
		// ��κ� ������ �� �ִ� TLS�� ���� ���. �ٸ����� ���Ǵ� TLS�� ���̴� �� �ܿ� ����� ����
		LOG(L"PROFILER", LOG_ERROR, L"TLS OUT OF INDEXES");
		CRASH(); 
		exit(0);
	}
}

CProfiler::~CProfiler()
{
	PrintData();
	TlsFree(_dwTlsIndex); // TLS ����
}

void CProfiler::Begin(WCHAR * szName)
{
	// TLS�κ��� ������ ���
	st_PROFILE_TLS *pInfo = (st_PROFILE_TLS*)TlsGetValue(_dwTlsIndex);
	if (pInfo == NULL)	// ù ����� ��� (TlsAlloc()�� ������ NULL�� �ʱ�ȭ��)
	{
		InterlockedIncrement(&_lTLSCnt);
		
		pInfo = &_stTLS[_lTLSCnt];
		pInfo->dwThreadID = GetCurrentThreadId();

		TlsSetValue(_dwTlsIndex, pInfo); // TLS�� ������ ����
	}

	// Begin Time ����
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
	// TLS�κ��� ������ ���
	st_PROFILE_TLS *pInfo = (st_PROFILE_TLS*)TlsGetValue(_dwTlsIndex);
	if (pInfo == NULL)	// ù ����� ��� (TlsAlloc()�� ������ NULL�� �ʱ�ȭ��)
	{
		LOG(L"PROFILER", LOG_ERROR, L"End() # Not Match with Begin()");
		CRASH();
	}

	// End Time ����
	LARGE_INTEGER lEndTime;
	QueryPerformanceCounter(&lEndTime);
	for (int iCnt = 0; iCnt < en_MAX_DATA; ++iCnt)
	{
		if (wcscmp(pInfo->stData[iCnt].szName, szName) == 0)
		{
			LONGLONG dTime = (lEndTime.QuadPart - pInfo->stData[iCnt].lBeginTime.QuadPart);
			pInfo->stData[iCnt].lBeginTime.QuadPart = 0;
			pInfo->stData[iCnt].dTotalTime += dTime;	// ���� �ð�

			// �ִ�, �ּ� �ð�
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

			pInfo->stData[iCnt].iCall++;	// ȣ�� Cnt
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
					swprintf_s(szString, 128, L"%20d|%20s|%18f��|%18f��|%18f��|%10lld   \r\n",
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