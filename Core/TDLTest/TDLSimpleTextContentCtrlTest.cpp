// TDLSimpleTextContentCtrlTest.cpp: implementation of the CTDLSimpleTextContentCtrlTest class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "TDLTest.h"
#include "TDLSimpleTextContentCtrlTest.h"

#include "..\todolist\tdlsimpletextcontentctrl.h"


#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////

struct EDITKEYCASE
{
	LPCTSTR szInitial;
	int cpMin;
	int cpMax;
	UINT nVKey;
	BOOL bShift;
	LPCTSTR szExpected;
};

// The contract from customized/text-editor/TED-004-comments-auto-indent.md
static const EDITKEYCASE EDITKEYCASES[] =
{
	// Enter continuation
	{ _T("  abc"),				5,	5,	VK_RETURN,	FALSE,	_T("  abc\n  ^") },
	{ _T("\tabc"),				4,	4,	VK_RETURN,	FALSE,	_T("\tabc\n\t^") },
	{ _T("abc"),				3,	3,	VK_RETURN,	FALSE,	_T("abc\n^") },
	{ _T("- item"),				6,	6,	VK_RETURN,	FALSE,	_T("- item\n^") },
	{ _T("  - item"),			8,	8,	VK_RETURN,	FALSE,	_T("  - item\n  ^") },
	{ _T("- "),					2,	2,	VK_RETURN,	FALSE,	_T("- \n^") },
	{ _T("  abcdef\nghi"),		5,	12,	VK_RETURN,	FALSE,	_T("  abc\n  ^") },

	// Return at the start of a line only adds an empty line
	{ _T("- item"),				0,	0,	VK_RETURN,	FALSE,	_T("\n^- item") },
	{ _T("  - item"),			0,	0,	VK_RETURN,	FALSE,	_T("\n^  - item") },
	{ _T("\t- item"),			0,	0,	VK_RETURN,	FALSE,	_T("\n^\t- item") },
	{ _T("  abc"),				0,	0,	VK_RETURN,	FALSE,	_T("\n^  abc") },
	{ _T("abc"),				0,	0,	VK_RETURN,	FALSE,	_T("\n^abc") },

	// Return copies only the leading whitespace, not the bullet marker
	{ _T("\t- item"),			7,	7,	VK_RETURN,	FALSE,	_T("\t- item\n\t^") },

	// Tab insertion
	{ _T("  abc"),				4,	4,	VK_TAB,		FALSE,	_T("  ab\t^c") },
	{ _T("a\nb"),				0,	3,	VK_TAB,		FALSE,	_T("\t[a\n\tb]") },
	{ _T("a\nb"),				0,	2,	VK_TAB,		FALSE,	_T("\t[a\n]b") },

	// Shift+Tab removes indentation from the caret line or the selection
	{ _T("\ta\n\tb"),			1,	5,	VK_TAB,		TRUE,	_T("[a\nb]") },
	{ _T("a\nb"),				0,	3,	VK_TAB,		TRUE,	_T("[a\nb]") },
	{ _T("\tabc"),				4,	4,	VK_TAB,		TRUE,	_T("abc^") },
	{ _T("\tabc"),				0,	0,	VK_TAB,		TRUE,	_T("^abc") },
	{ _T("\tabc"),				0,	4,	VK_TAB,		TRUE,	_T("[abc]") },
	{ _T("  abc"),				5,	5,	VK_TAB,		TRUE,	_T("abc^") },
	{ _T("\t- item"),			7,	7,	VK_TAB,		TRUE,	_T("- item^") },
};

static CString EscapeText(LPCTSTR szText)
{
	CString sOut(szText);

	sOut.Replace(_T("\\"), _T("\\\\"));
	sOut.Replace(_T("\r"), _T("\\r"));
	sOut.Replace(_T("\n"), _T("\\n"));
	sOut.Replace(_T("\t"), _T("\\t"));

	return sOut;
}

// Converts the spec's \n line breaks to the RichEdit \r form
static CString NormalizeLineBreaks(LPCTSTR szText)
{
	CString sText(szText);
	sText.Replace(_T('\n'), _T('\r'));

	return sText;
}

// Applies the spec's normalised caret offsets to the control
static void SetEditorTextAndSelection(CTDLSimpleTextContentCtrl& ctrl, LPCTSTR szText, int cpMin, int cpMax)
{
	ctrl.SetTextContent(NormalizeLineBreaks(szText), TRUE);
	ctrl.SetSel(cpMin, cpMax);
}

//////////////////////////////////////////////////////////////////////

CTDLSimpleTextContentCtrlTest::CTDLSimpleTextContentCtrlTest(const CTestUtils& utils)
	:
	CTDLTestBase(_T("CTDLSimpleTextContentCtrlTest"), utils)
{
}

CTDLSimpleTextContentCtrlTest::~CTDLSimpleTextContentCtrlTest()
{
}

TESTRESULT CTDLSimpleTextContentCtrlTest::Run()
{
	ClearTotals();

	if (CreateHost())
	{
		TestDefaults();
		TestSpaceIndent();
		TestSynchronousFormatting();
		TestFormattingScope();
		TestRepeatedIndentKeys();
		TestEditingKeys();
		TestImmediateIndentUpdate();
		TestWrappedLineAlignment();
		TestCustomBulletPrefixes();
	}
	else
	{
		CTDCScopedTest test(*this, _T("CreateTestControl"));
		ExpectTrue(FALSE);
	}

	return GetTotals();
}

BOOL CTDLSimpleTextContentCtrlTest::CreateHost()
{
	if (!m_font.GetSafeHandle())
		m_font.CreatePointFont(100, _T("Courier New"));

	return m_wndHost.CreateEx(0, AfxRegisterWndClass(0), _T("TDLTestHost"),
							  WS_OVERLAPPEDWINDOW, CRect(0, 0, 1000, 800), NULL, 0);
}

BOOL CTDLSimpleTextContentCtrlTest::CreateEditor(CTDLSimpleTextContentCtrl& ctrl)
{
	if (!ctrl.Create(WS_CHILD | WS_VISIBLE, CRect(0, 0, 400, 300), &m_wndHost, 100))
		return FALSE;

	ctrl.SetContentFont((HFONT)m_font);
	ctrl.SendMessage(WM_SETWORDWRAP, TRUE, (LPARAM)ctrl.GetSafeHwnd());

	return TRUE;
}

void CTDLSimpleTextContentCtrlTest::TestDefaults()
{
	CTDCScopedTest test(*this, _T("CTDLSimpleTextContentCtrl::Defaults"));

	// Inline spell checking is off until a preference explicitly enables it
	ExpectFalse(CTDLSimpleTextContentCtrl::IsInlineSpellCheckingEnabled());

	// Spaces are inserted by default
	ExpectTrue(CTDLSimpleTextContentCtrl::IsTabIndentWithSpaces());

	CTDLSimpleTextContentCtrl ctrl;

	if (!ExpectTrue(CreateEditor(ctrl)))
		return;

	// Word wrap is on by default
	ExpectTrue(ctrl.SendMessage(WM_GETWORDWRAP) != 0);
}

void CTDLSimpleTextContentCtrlTest::TestSpaceIndent()
{
	CTDCScopedTest test(*this, _T("CTDLSimpleTextContentCtrl::SpaceIndent"));

	CTDLSimpleTextContentCtrl ctrl;

	if (!ExpectTrue(CreateEditor(ctrl)))
		return;

	CTDLSimpleTextContentCtrl::SetTabIndentWithSpaces(TRUE);

	// Caret Tab inserts two spaces
	SetEditorTextAndSelection(ctrl, _T("abc"), 0, 0);
	ExpectTrue(ctrl.ProcessEditingKey(VK_TAB, FALSE, FALSE, FALSE));
	ExpectEQ(GetMarkedText(ctrl), _T("  ^abc"));

	// Multi-line selection
	SetEditorTextAndSelection(ctrl, _T("a\nb"), 0, 3);
	ExpectTrue(ctrl.ProcessEditingKey(VK_TAB, FALSE, FALSE, FALSE));
	ExpectEQ(GetMarkedText(ctrl), _T("  [a\n  b]"));

	// Selection ending at a line start
	SetEditorTextAndSelection(ctrl, _T("a\nb"), 0, 2);
	ExpectTrue(ctrl.ProcessEditingKey(VK_TAB, FALSE, FALSE, FALSE));
	ExpectEQ(GetMarkedText(ctrl), _T("  [a\n]b"));

	// Existing tabs are untouched; Shift+Tab removes the inserted spaces
	SetEditorTextAndSelection(ctrl, _T("\tabc"), 0, 0);
	ExpectTrue(ctrl.ProcessEditingKey(VK_TAB, FALSE, FALSE, FALSE));
	ExpectEQ(GetMarkedText(ctrl), _T("  ^\tabc"));

	ExpectTrue(ctrl.ProcessEditingKey(VK_TAB, FALSE, FALSE, TRUE));
	ExpectEQ(GetMarkedText(ctrl), _T("^\tabc"));

	CTDLSimpleTextContentCtrl::SetTabIndentWithSpaces(FALSE);
}

void CTDLSimpleTextContentCtrlTest::TestSynchronousFormatting()
{
	CTDCScopedTest test(*this, _T("CTDLSimpleTextContentCtrl::SynchronousFormatting"));

	CTDLSimpleTextContentCtrl ctrl;

	if (!ExpectTrue(CreateEditor(ctrl)))
		return;

	PARAFORMAT2 pf;
	ZeroMemory(&pf, sizeof(pf));
	pf.cbSize = sizeof(pf);

	// Loading text applies the hanging indent before returning
	ctrl.SetTextContent(_T("- item"), TRUE);

	ctrl.SetSel(0, 0);
	ctrl.GetParaFormat(pf);
	ExpectTrue(pf.dxOffset > 0);

	// Editing the prefix updates the caret paragraph immediately
	ctrl.SetSel(0, 0);
	ctrl.ReplaceSel(_T("  "), TRUE);
	ctrl.OnChangeText();

	ctrl.SetSel(0, 0);
	ZeroMemory(&pf, sizeof(pf));
	pf.cbSize = sizeof(pf);
	ctrl.GetParaFormat(pf);

	CPoint ptStart, ptText;
	ctrl.PointFromChar(0, ptStart);
	ctrl.PointFromChar(4, ptText);

	CClientDC dc(&ctrl);
	const int nDpiX = dc.GetDeviceCaps(LOGPIXELSX);
	const int nExpected = ((ptText.x - ptStart.x) * 1440 / nDpiX);

	ExpectEQ((int)pf.dxOffset, nExpected);

	// Multi-line insertion re-indents all inserted paragraphs
	ctrl.SetTextContent(_T(""), TRUE);
	ExpectTrue(ctrl.InsertTextContent(_T("- one\r- two"), TRUE));

	ctrl.SetSel(0, 0);
	ZeroMemory(&pf, sizeof(pf));
	pf.cbSize = sizeof(pf);
	ctrl.GetParaFormat(pf);
	ExpectTrue(pf.dxOffset > 0);

	ctrl.SetSel(6, 6);
	ZeroMemory(&pf, sizeof(pf));
	pf.cbSize = sizeof(pf);
	ctrl.GetParaFormat(pf);
	ExpectTrue(pf.dxOffset > 0);

	// Undo/redo run the full pass on the restored text
	ctrl.SetTextContent(_T("abc"), TRUE);
	ctrl.SetSel(3, 3);
	ctrl.ReplaceSel(_T("\r- item"), TRUE);

	ExpectTrue(ctrl.Undo());

	CString sUndo = ctrl.GetTextRange(CHARRANGE{ 0, ctrl.GetTextLength() });
	ExpectEQ(sUndo, _T("abc"));

	ExpectTrue(ctrl.Redo());

	CString sRedo = ctrl.GetTextRange(CHARRANGE{ 0, ctrl.GetTextLength() });
	ExpectEQ(sRedo, _T("abc\r- item"));

	ctrl.SetSel(4, 4);
	ZeroMemory(&pf, sizeof(pf));
	pf.cbSize = sizeof(pf);
	ctrl.GetParaFormat(pf);
	ExpectTrue(pf.dxOffset > 0);
}

void CTDLSimpleTextContentCtrlTest::TestFormattingScope()
{
	CTDCScopedTest test(*this, _T("CTDLSimpleTextContentCtrl::FormattingScope"));

	CTDLSimpleTextContentCtrl ctrl;

	if (!ExpectTrue(CreateEditor(ctrl)))
		return;

	CString sDoc;

	for (int nLine = 0; nLine < 10; nLine++)
		sDoc += _T("- item\r");

	// Ordinary typing formats only the caret paragraph
	ctrl.SetTextContent(sDoc, TRUE);
	ctrl.SetSel(ctrl.GetTextLength(), ctrl.GetTextLength());
	ctrl.ReplaceSel(_T("x"), TRUE);

	ctrl.m_nNumParagraphsFormatted = 0;
	ctrl.OnChangeText();
	ExpectEQ(ctrl.m_nNumParagraphsFormatted, 1);

	// Tab with a caret formats only its paragraph, regardless of document size
	ctrl.SetTextContent(sDoc, TRUE);
	ctrl.SetSel(0, 0);

	ctrl.m_nNumParagraphsFormatted = 0;
	ExpectTrue(ctrl.ProcessEditingKey(VK_TAB, FALSE, FALSE, FALSE));
	ExpectEQ(ctrl.m_nNumParagraphsFormatted, 1);

	// Return at a line start formats only the two affected paragraphs
	ctrl.SetTextContent(sDoc, TRUE);
	ctrl.SetSel(0, 0);

	ctrl.m_nNumParagraphsFormatted = 0;
	ExpectTrue(ctrl.ProcessEditingKey(VK_RETURN, FALSE, FALSE, FALSE));
	ExpectTrue(ctrl.m_nNumParagraphsFormatted <= 2);

	// Tab on a selection formats only the touched paragraphs
	ctrl.SetTextContent(sDoc, TRUE);
	ctrl.SetSel(0, 10);

	ctrl.m_nNumParagraphsFormatted = 0;
	ExpectTrue(ctrl.ProcessEditingKey(VK_TAB, FALSE, FALSE, FALSE));
	ExpectTrue(ctrl.m_nNumParagraphsFormatted <= 3);
}

void CTDLSimpleTextContentCtrlTest::TestRepeatedIndentKeys()
{
	CTDCScopedTest test(*this, _T("CTDLSimpleTextContentCtrl::RepeatedIndentKeys"));

	CTDLSimpleTextContentCtrl ctrl;

	if (!ExpectTrue(CreateEditor(ctrl)))
		return;

	ctrl.SetTextContent(_T("abc"), TRUE);
	ctrl.SetSel(0, 0);

	// Each Tab keydown is applied immediately, including auto-repeat
	for (int nTab = 1; nTab <= 4; nTab++)
	{
		ExpectTrue(ctrl.ProcessEditingKey(VK_TAB, FALSE, FALSE, FALSE));

		ExpectEQ(ctrl.GetTextRange(CHARRANGE{ 0, nTab }), CString(_T('\t'), nTab));

		CHARRANGE cr;
		ctrl.GetSel(cr);
		ExpectEQ((int)cr.cpMin, nTab);
	}

	// And each Shift+Tab removes one level immediately
	for (int nTab = 3; nTab >= 0; nTab--)
	{
		ExpectTrue(ctrl.ProcessEditingKey(VK_TAB, FALSE, FALSE, TRUE));

		ExpectEQ(ctrl.GetTextRange(CHARRANGE{ 0, nTab }), CString(_T('\t'), nTab));

		CHARRANGE cr;
		ctrl.GetSel(cr);
		ExpectEQ((int)cr.cpMin, nTab);
	}
}

void CTDLSimpleTextContentCtrlTest::TestEditingKeys()
{
	CTDCScopedTest test(*this, _T("CTDLSimpleTextContentCtrl::EditingKeys"));

	CTDLSimpleTextContentCtrl ctrl;

	if (!ExpectTrue(CreateEditor(ctrl)))
		return;

	const int nNumCases = _countof(EDITKEYCASES);

	for (int nCase = 0; nCase < nNumCases; nCase++)
	{
		const EDITKEYCASE& testCase = EDITKEYCASES[nCase];

		SetEditorTextAndSelection(ctrl, testCase.szInitial, testCase.cpMin, testCase.cpMax);

		ExpectTrue(ctrl.ProcessEditingKey(testCase.nVKey, FALSE, FALSE, testCase.bShift));

		CString sActual = GetMarkedText(ctrl);
		CString sExpected(testCase.szExpected);

		if (sActual != sExpected)
		{
			_tprintf(_T("    case %d: initial='%s' key=%s shift=%d expected='%s' actual='%s'\n"),
					 nCase, EscapeText(testCase.szInitial),
					 (testCase.nVKey == VK_RETURN) ? _T("Return") : _T("Tab"),
					 testCase.bShift, EscapeText(sExpected), EscapeText(sActual));
		}

		ExpectEQ(sActual, sExpected);
	}

	// Shift+Enter and unhandled keys must not be intercepted
	ctrl.SetTextContent(_T("abc"), TRUE);
	ctrl.SetSel(3, 3);

	ExpectFalse(ctrl.ProcessEditingKey(VK_RETURN, FALSE, FALSE, TRUE));
	ExpectFalse(ctrl.ProcessEditingKey('A', FALSE, FALSE, FALSE));
}

void CTDLSimpleTextContentCtrlTest::TestImmediateIndentUpdate()
{
	CTDCScopedTest test(*this, _T("CTDLSimpleTextContentCtrl::ImmediateIndentUpdate"));

	CTDLSimpleTextContentCtrl ctrl;

	if (!ExpectTrue(CreateEditor(ctrl)))
		return;

	ctrl.SetTextContent(_T("-> item"), TRUE);
	ctrl.ApplyAutoIndent();

	PARAFORMAT2 pf;
	ZeroMemory(&pf, sizeof(pf));
	pf.cbSize = sizeof(pf);

	ctrl.SetSel(0, 0);
	ctrl.GetParaFormat(pf);

	ExpectTrue(pf.dxOffset > 0);

	// Inserting a leading tab must clear the hanging indent before the
	// control is next painted, otherwise the tab visibly jumps
	ExpectTrue(ctrl.ProcessEditingKey(VK_TAB, FALSE, FALSE, FALSE));

	ctrl.SetSel(0, 0);
	ZeroMemory(&pf, sizeof(pf));
	pf.cbSize = sizeof(pf);
	ctrl.GetParaFormat(pf);

	ExpectEQ((int)pf.dxOffset, 0);

	CString sAfter = ctrl.GetTextRange(CHARRANGE{ 0, ctrl.GetTextLength() });
	ExpectEQ(sAfter, _T("\t-> item"));
}

void CTDLSimpleTextContentCtrlTest::TestWrappedLineAlignment()
{
	CTDCScopedTest test(*this, _T("CTDLSimpleTextContentCtrl::WrappedLineAlignment"));

	CTDLSimpleTextContentCtrl ctrl;

	if (!ExpectTrue(CreateEditor(ctrl)))
		return;

	CString sLong;

	for (int nRepeat = 0; nRepeat < 10; nRepeat++)
		sLong += _T("The quick brown fox jumps over the lazy dog. ");

	// Space indented paragraphs align wrapped lines under the first text char
	{
		ctrl.SetTextContent((_T("  ") + sLong), TRUE);
		ctrl.ApplyAutoIndent();

		const int nContStart = (int)ctrl.SendMessage(EM_LINEINDEX, 1, 0);
		ExpectTrue(nContStart > 0);

		CPoint ptText, ptCont;
		ctrl.PointFromChar(2, ptText);
		ctrl.PointFromChar(nContStart, ptCont);

		ExpectTrue(abs(ptCont.x - ptText.x) <= 1);
	}

	// Space indented bullets align under the text after the bullet
	{
		ctrl.SetTextContent((_T("- ") + sLong), TRUE);
		ctrl.ApplyAutoIndent();

		const int nContStart = (int)ctrl.SendMessage(EM_LINEINDEX, 1, 0);
		ExpectTrue(nContStart > 0);

		CPoint ptText, ptCont;
		ctrl.PointFromChar(2, ptText);
		ctrl.PointFromChar(nContStart, ptCont);

		ExpectTrue(abs(ptCont.x - ptText.x) <= 1);
	}

	// Tab indented paragraphs cannot be hang-indented by RichEdit 4.1,
	// so the continuation stays at the paragraph start
	{
		ctrl.SetTextContent((_T("\t") + sLong), TRUE);
		ctrl.ApplyAutoIndent();

		const int nContStart = (int)ctrl.SendMessage(EM_LINEINDEX, 1, 0);
		ExpectTrue(nContStart > 0);

		CPoint ptPara, ptText, ptCont;
		ctrl.PointFromChar(0, ptPara);
		ctrl.PointFromChar(1, ptText);
		ctrl.PointFromChar(nContStart, ptCont);

		ExpectEQ(ptCont.x, ptPara.x);
		ExpectTrue(ptCont.x < ptText.x);

		// Tab indentation must be left in the text so that Shift+Tab works
		CString sAfter = ctrl.GetTextRange(CHARRANGE{ 0, ctrl.GetTextLength() });
		ExpectEQ(sAfter, (_T("\t") + sLong));
	}
}

void CTDLSimpleTextContentCtrlTest::TestCustomBulletPrefixes()
{
	CTDCScopedTest test(*this, _T("CTDLSimpleTextContentCtrl::CustomBulletPrefixes"));

	CTDLSimpleTextContentCtrl ctrl;

	if (!ExpectTrue(CreateEditor(ctrl)))
		return;

	CStringArray aDefaults;
	ExpectEQ(CTDLSimpleTextContentCtrl::GetDefaultBulletPrefixes(aDefaults), 3);

	CStringArray aPrefixes;
	aPrefixes.Add(_T("* "));
	aPrefixes.Add(_T("# "));
	CTDLSimpleTextContentCtrl::SetBulletPrefixes(aPrefixes);

	CString sLong;

	for (int nRepeat = 0; nRepeat < 10; nRepeat++)
		sLong += _T("The quick brown fox jumps over the lazy dog. ");

	ctrl.SetTextContent((_T("* ") + sLong), TRUE);
	ctrl.ApplyAutoIndent();

	const int nContStart = (int)ctrl.SendMessage(EM_LINEINDEX, 1, 0);
	ExpectTrue(nContStart > 0);

	CPoint ptText, ptCont;
	ctrl.PointFromChar(2, ptText);
	ctrl.PointFromChar(nContStart, ptCont);

	ExpectTrue(abs(ptCont.x - ptText.x) <= 1);

	// Restore the defaults for any later tests
	CTDLSimpleTextContentCtrl::SetBulletPrefixes(aDefaults);
}

CString CTDLSimpleTextContentCtrlTest::GetMarkedText(const CTDLSimpleTextContentCtrl& ctrl) const
{
	// Use the control's character positions, which use a single CR
	// for each line break, unlike GetWindowText which returns CRLF
	CString sText = ctrl.GetTextRange(CHARRANGE{ 0, ctrl.GetTextLength() });
	sText.Replace(_T('\r'), _T('\n'));

	CHARRANGE cr;
	ctrl.GetSel(cr);

	cr.cpMin = max(0, min(sText.GetLength(), cr.cpMin));
	cr.cpMax = max(0, min(sText.GetLength(), cr.cpMax));

	if (cr.cpMin == cr.cpMax)
	{
		sText.Insert(cr.cpMin, _T('^'));
	}
	else
	{
		sText.Insert(cr.cpMax, _T(']'));
		sText.Insert(cr.cpMin, _T('['));
	}

	return sText;
}





