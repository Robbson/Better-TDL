// TDLSimpleTextContentCtrlTest.h: interface for the CTDLSimpleTextContentCtrlTest class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_TDLSIMPLETEXTCONTENTCTRLTEST_H__6E5B4B0F_7A3C_4E6E_9C4B_8F5D1E2A3B4C__INCLUDED_)
#define AFX_TDLSIMPLETEXTCONTENTCTRLTEST_H__6E5B4B0F_7A3C_4E6E_9C4B_8F5D1E2A3B4C__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "TDLTestBase.h"

class CTDLSimpleTextContentCtrl;

class CTDLSimpleTextContentCtrlTest : public CTDLTestBase
{
public:
	CTDLSimpleTextContentCtrlTest(const CTestUtils& utils);
	virtual ~CTDLSimpleTextContentCtrlTest();

	TESTRESULT Run();

protected:
	BOOL CreateHost();
	BOOL CreateEditor(CTDLSimpleTextContentCtrl& ctrl);

	void TestDefaults();
	void TestSpaceIndent();
	void TestSynchronousFormatting();
	void TestFormattingScope();
	void TestRepeatedIndentKeys();
	void TestEditingKeys();
	void TestImmediateIndentUpdate();
	void TestWrappedLineAlignment();

	CString GetMarkedText(const CTDLSimpleTextContentCtrl& ctrl) const;

protected:
	CWnd m_wndHost;
	CFont m_font;
};

#endif // !defined(AFX_TDLSIMPLETEXTCONTENTCTRLTEST_H__6E5B4B0F_7A3C_4E6E_9C4B_8F5D1E2A3B4C__INCLUDED_)


