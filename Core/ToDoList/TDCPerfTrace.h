#ifndef TDC_PERF_TRACE_H
#define TDC_PERF_TRACE_H

// Buffered scope timer for attributing tasklist activation cost.
// Unlike CScopedLogTimer, every scope is accumulated in memory and written
// to the log in a single append when the outermost scope ends. This keeps
// the measurement itself from dominating the cost being measured.
//
// Only active when FileMisc logging is enabled. Compatible with the subset
// of the CScopedLogTimer interface used by the activation path.

#include "..\Shared\FileMisc.h"

class CTDCPerfTrace
{
public:
	CTDCPerfTrace();
	CTDCPerfTrace(LPCTSTR szScope, LPCTSTR szArg1 = NULL, LPCTSTR szArg2 = NULL, LPCTSTR szArg3 = NULL);
	~CTDCPerfTrace();

	void LogStart() {}
	void LogTimeElapsed(LPCTSTR szSubScope, LPCTSTR szArg1 = NULL, LPCTSTR szArg2 = NULL, LPCTSTR szArg3 = NULL);

protected:
	struct TRACE_STATE
	{
		TRACE_STATE() : nDepth(0), llFreq(0), llRoot(0)
		{
			LARGE_INTEGER li;
			::QueryPerformanceFrequency(&li);
			llFreq = li.QuadPart;
		}

		int nDepth;
		LONGLONG llFreq;
		LONGLONG llRoot;
		CStringArray aLines;
	};

	static TRACE_STATE& State()
	{
		static TRACE_STATE s_state;
		return s_state;
	}

	static LONGLONG Now()
	{
		LARGE_INTEGER li;
		::QueryPerformanceCounter(&li);
		return li.QuadPart;
	}

	static CString FormatScope(LPCTSTR szScope, LPCTSTR szArg1, LPCTSTR szArg2, LPCTSTR szArg3)
	{
		CString sScope(szScope ? szScope : _T(""));

		if (szScope)
		{
			if (szArg1 && szArg2 && szArg3)
				sScope.Format(szScope, szArg1, szArg2, szArg3);
			else if (szArg1 && szArg2)
				sScope.Format(szScope, szArg1, szArg2);
			else if (szArg1)
				sScope.Format(szScope, szArg1);
		}

		return sScope;
	}

	void AddLine(LPCTSTR szName, LONGLONG llOffset, LONGLONG llElapsed);

protected:
	BOOL m_bActive;
	int m_nDepth;
	LONGLONG m_llStart;
	LONGLONG m_llLast;
	CString m_sScope;
};

inline CTDCPerfTrace::CTDCPerfTrace()
	: m_bActive(FALSE), m_nDepth(0), m_llStart(0), m_llLast(0)
{
	if (!FileMisc::IsLoggingEnabled())
		return;

	TRACE_STATE& state = State();

	if (state.nDepth == 0)
	{
		state.llRoot = Now();
		state.aLines.RemoveAll();
	}

	m_bActive = TRUE;
	m_nDepth = state.nDepth;
	state.nDepth++;

	m_llStart = m_llLast = Now();
}

inline CTDCPerfTrace::CTDCPerfTrace(LPCTSTR szScope, LPCTSTR szArg1, LPCTSTR szArg2, LPCTSTR szArg3)
	: m_bActive(FALSE), m_nDepth(0), m_llStart(0), m_llLast(0)
{
	if (!FileMisc::IsLoggingEnabled())
		return;

	m_sScope = FormatScope(szScope, szArg1, szArg2, szArg3);

	TRACE_STATE& state = State();

	if (state.nDepth == 0)
	{
		state.llRoot = Now();
		state.aLines.RemoveAll();
	}

	m_bActive = TRUE;
	m_nDepth = state.nDepth;
	state.nDepth++;

	m_llStart = m_llLast = Now();
}

inline CTDCPerfTrace::~CTDCPerfTrace()
{
	if (!m_bActive)
		return;

	TRACE_STATE& state = State();
	LONGLONG llNow = Now();

	AddLine(m_sScope.IsEmpty() ? _T("<scope>") : (LPCTSTR)m_sScope, m_llStart - state.llRoot, llNow - m_llStart);

	if (--state.nDepth == 0)
	{
		CString sAll;

		for (INT_PTR nLine = 0; nLine < state.aLines.GetSize(); nLine++)
		{
			if (nLine > 0)
				sAll += _T("\r\n");

			sAll += state.aLines[nLine];
		}

		if (!sAll.IsEmpty())
			FileMisc::LogTextRaw(sAll);

		state.aLines.RemoveAll();
	}
}

inline void CTDCPerfTrace::LogTimeElapsed(LPCTSTR szSubScope, LPCTSTR szArg1, LPCTSTR szArg2, LPCTSTR szArg3)
{
	if (!m_bActive || !szSubScope)
		return;

	LONGLONG llNow = Now();
	CString sName = FormatScope(szSubScope, szArg1, szArg2, szArg3);

	AddLine(sName, m_llLast - State().llRoot, llNow - m_llLast);
	m_llLast = llNow;
}

inline void CTDCPerfTrace::AddLine(LPCTSTR szName, LONGLONG llOffset, LONGLONG llElapsed)
{
	TRACE_STATE& state = State();

	if (state.llFreq <= 0)
		return;

	CString sIndent;
	
	for (int nIndent = 0; nIndent < m_nDepth; nIndent++)
		sIndent += _T("  ");

	CString sLine;
	sLine.Format(_T("%s%s took %.2f ms (at +%.2f ms)"),
				(LPCTSTR)sIndent, szName,
				(llElapsed * 1000.0) / state.llFreq,
				(llOffset * 1000.0) / state.llFreq);

	state.aLines.Add(sLine);
}

#endif // TDC_PERF_TRACE_H
