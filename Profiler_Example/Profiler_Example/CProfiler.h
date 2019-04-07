/*---------------------------------------------------------------
Log

로그 클래스.
파일 및 콘솔에 로그 출력


- 사용법

#ifdef __PROFILER__
#define BEGIN(szName) Profiler.Begin(szName)
#define END(szName) Profiler.End(szName)
#define PRINT() Profiler.PrintData()
#else
#define BEGIN(szName) {}
#define END(szName) {}
#define PRINT() {}
#endif
----------------------------------------------------------------*/
#ifndef __PROFILER__
#define __PROFILER__
#include "CrashDump.h"
#include "CSystemLog.h"
#include <locale.h>
#include <Windows.h>

class CProfiler
{
private:
	enum en_PROFILER
	{
		en_MAX_THREAD = 20,
		en_MAX_DATA = 60
	};
	struct st_PROFILE_DATA
	{
		bool bUse;

		WCHAR szName[64];
		LARGE_INTEGER lBeginTime;

		__int64 iCall;		// 호출 횟수
		double dTotalTime;	// 누적 시간
		double dMin[2];		// 최소 시간
		double dMax[2];		// 최대 시간
		// 프로세스를 처음 시작한다던지 때때로 터무니없이 낮거나 높은 시간이 나올 수 있다.
		// 때문에 최하, 최상 1~2개 정도는 버리는 것이 좋다.
	};

	struct st_PROFILE_TLS
	{
		DWORD			dwThreadID;
		st_PROFILE_DATA	stData[en_MAX_DATA];	
	};

public:
	CProfiler();
	~CProfiler();

	void	Begin(WCHAR *szName);
	void	End(WCHAR *szName);
	void	PrintData();

private:
	st_PROFILE_TLS		_stTLS[en_MAX_THREAD];

	// Time
	LARGE_INTEGER	_IFrequency;	// 초당 count
	LARGE_INTEGER	_lCurCnt;		// 현재 count
	int				_iMicroSec;


	DWORD			_dwTlsIndex;
	LONG			_lTLSCnt;
};

extern  CProfiler Profiler;
#endif