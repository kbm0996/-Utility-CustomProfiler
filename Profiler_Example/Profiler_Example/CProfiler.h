/*---------------------------------------------------------------
Log

�α� Ŭ����.
���� �� �ֿܼ� �α� ���


- ����

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

		__int64 iCall;		// ȣ�� Ƚ��
		double dTotalTime;	// ���� �ð�
		double dMin[2];		// �ּ� �ð�
		double dMax[2];		// �ִ� �ð�
		// ���μ����� ó�� �����Ѵٴ��� ������ �͹��Ͼ��� ���ų� ���� �ð��� ���� �� �ִ�.
		// ������ ����, �ֻ� 1~2�� ������ ������ ���� ����.
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
	LARGE_INTEGER	_IFrequency;	// �ʴ� count
	LARGE_INTEGER	_lCurCnt;		// ���� count
	int				_iMicroSec;


	DWORD			_dwTlsIndex;
	LONG			_lTLSCnt;
};

extern  CProfiler Profiler;
#endif