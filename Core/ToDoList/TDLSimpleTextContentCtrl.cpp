// tdlcommentsctrl.cpp : implementation file
//

#include "stdafx.h"
#include "TDLSimpleTextContentCtrl.h"
#include "tdcstatic.h"
#include "tdcenum.h"
#include "resource.h"

#include "..\shared\toolbarhelper.h"
#include "..\shared\enfiledialog.h"
#include "..\shared\wclassdefines.h"
#include "..\shared\autoflag.h"
#include "..\shared\richedithelper.h"
#include "..\shared\misc.h"
#include "..\shared\webmisc.h"
#include "..\shared\enstring.h"
#include "..\shared\filemisc.h"
#include "..\shared\enmenu.h"
#include "..\shared\clipboard.h"
#include "..\shared\localizer.h"
#include "..\shared\msoutlookhelper.h"
#include "..\shared\dialoghelper.h"
#include "..\shared\holdredraw.h"

#include "..\3rdParty\OSVersion.h"
#include "..\3rdParty\XNamedColors.h"

#include "..\Interfaces\ipreferences.h"
#include "..\Interfaces\ITaskList.h"
#include "..\Interfaces\richeditspellcheck.h"
#include "..\Interfaces\TasklistSchemaDef.h"

/////////////////////////////////////////////////////////////////////////////

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////

const UINT IDC_DROPLIST = 100;

const int MAX_AUTOINDENT_LENGTH = (1024 * 1024);

const DWORD SIMPLETEXT_COMMENTS = (DWORD)(LPCTSTR)_T("SIMPLETEXT_COMMENTS");

/////////////////////////////////////////////////////////////////////////////

BOOL CTDLSimpleTextContentCtrl::s_bInlineSpellChecking = FALSE;
BOOL CTDLSimpleTextContentCtrl::s_bPasteSourceUrls = TRUE;
BOOL CTDLSimpleTextContentCtrl::s_bTabIndentWithSpaces = TRUE;
CStringArray CTDLSimpleTextContentCtrl::s_aBulletPrefixes;
BOOL CTDLSimpleTextContentCtrl::s_bBulletPrefixesSet = FALSE;

/////////////////////////////////////////////////////////////////////////////

static const CLIPFORMAT CF_PREFERRED[] = 
{ 
	CF_HDROP,
	CB_TEXTFORMAT,
};
const long NUM_PREF = sizeof(CF_PREFERRED) / sizeof(CLIPFORMAT);

/////////////////////////////////////////////////////////////////////////////

// Default bullet markers, editable in Preferences
static const LPCTSTR BULLET_PREFIXES[] =
{
	_T("- "),
	//_T("* "),
	//_T("+ "),
	_T("-> "),
	_T("=> "),
};

/////////////////////////////////////////////////////////////////////////////

int CTDLSimpleTextContentCtrl::GetDefaultBulletPrefixes(CStringArray& aPrefixes)
{
	aPrefixes.RemoveAll();

	for (int nPrefix = 0; nPrefix < _countof(BULLET_PREFIXES); nPrefix++)
		aPrefixes.Add(BULLET_PREFIXES[nPrefix]);

	return aPrefixes.GetSize();
}

void CTDLSimpleTextContentCtrl::SetBulletPrefixes(const CStringArray& aPrefixes)
{
	s_aBulletPrefixes.Copy(aPrefixes);
	s_bBulletPrefixesSet = TRUE;
}

const CStringArray& CTDLSimpleTextContentCtrl::GetBulletPrefixes()
{
	if (!s_bBulletPrefixesSet)
	{
		GetDefaultBulletPrefixes(s_aBulletPrefixes);
		s_bBulletPrefixesSet = TRUE;
	}

	return s_aBulletPrefixes;
}

/////////////////////////////////////////////////////////////////////////////

static int FindParagraphStart(const CRichEditBaseCtrl& ctrl, int nPos)
{
	const int CHUNK = 1024;

	while (nPos > 0)
	{
		int nChunkStart = max(0, nPos - CHUNK);
		CHARRANGE cr = { nChunkStart, nPos };

		CString sChunk = ctrl.GetTextRange(cr);
		int nBreak = max(sChunk.ReverseFind(_T('\r')), sChunk.ReverseFind(_T('\n')));

		if (nBreak >= 0)
			return (nChunkStart + nBreak + 1);

		nPos = nChunkStart;
	}

	return 0;
}

static int FindParagraphEnd(const CRichEditBaseCtrl& ctrl, int nParaStart, int nTextLen)
{
	const int CHUNK = 1024;
	int nPos = nParaStart;

	while (nPos < nTextLen)
	{
		int nChunkEnd = min((nPos + CHUNK), nTextLen);
		CHARRANGE crChunk = { nPos, nChunkEnd };

		CString sChunk = ctrl.GetTextRange(crChunk);
		int nBreak = sChunk.FindOneOf(_T("\r\n"));

		if (nBreak >= 0)
			return (nPos + nBreak);

		nPos = nChunkEnd;
	}

	return nTextLen;
}

static int GetBulletPrefixLength(const CString& sText, int nStart)
{
	const CStringArray& aPrefixes = CTDLSimpleTextContentCtrl::GetBulletPrefixes();
	int nNumPrefixes = aPrefixes.GetSize();

	for (int nPrefix = 0; nPrefix < nNumPrefixes; nPrefix++)
	{
		const CString& sPrefix = aPrefixes[nPrefix];
		int nLen = sPrefix.GetLength();

		if (((nStart + nLen) <= sText.GetLength()) &&
			(sText.Mid(nStart, nLen) == sPrefix))
		{
			return nLen;
		}
	}

	return 0;
}

static int FindLineStartInText(const CString& sText, int nPos)
{
	int nStart = nPos;

	while ((nStart > 0) && (sText[nStart - 1] != _T('\n')) && (sText[nStart - 1] != _T('\r')))
	{
		nStart--;
	}

	return nStart;
}

// Returns the length of the indentation to remove from a line start:
// one tab or up to two spaces (the editor's tab width).
static int GetRemovableIndentLength(const CString& sText, int nStart)
{
	if ((nStart < 0) || (nStart >= sText.GetLength()))
		return 0;

	if (sText[nStart] == _T('\t'))
		return 1;

	int nSpaces = 0;

	while ((nSpaces < 2) && ((nStart + nSpaces) < sText.GetLength()) &&
		   (sText[nStart + nSpaces] == _T(' ')))
	{
		nSpaces++;
	}

	return nSpaces;
}

static void GetParagraphPrefix(const CString& sPara, int& nFirstNonWs, int& nBulletLen, int& nTextStart){
	nFirstNonWs = 0;

	while ((nFirstNonWs < sPara.GetLength()) &&
		   ((sPara[nFirstNonWs] == _T(' ')) || (sPara[nFirstNonWs] == _T('\t'))))
	{
		nFirstNonWs++;
	}

	nTextStart = nFirstNonWs;
	nBulletLen = GetBulletPrefixLength(sPara, nFirstNonWs);

	if (nBulletLen > 0)
	{
		nTextStart += nBulletLen;

		// skip any extra spaces/tabs after the marker
		while ((nTextStart < sPara.GetLength()) &&
			   ((sPara[nTextStart] == _T(' ')) || (sPara[nTextStart] == _T('\t'))))
		{
			nTextStart++;
		}
	}
	else if (nFirstNonWs == sPara.GetLength())
	{
		nTextStart = 0; // whitespace-only paragraph has nothing to align to
	}
}

static CString GetAutoIndentPrefix(const CRichEditBaseCtrl& ctrl, int nPos)
{
	const int CHUNK = 1024;
	const int nTextLen = ctrl.GetTextLength();

	int nParaStart = FindParagraphStart(ctrl, nPos);
	int nParaEnd = FindParagraphEnd(ctrl, nParaStart, nTextLen);
	int nChunkEnd = min((nParaStart + CHUNK), nParaEnd);
	CHARRANGE crLead = { nParaStart, nChunkEnd };
	CString sLead = ctrl.GetTextRange(crLead);

	int nIndentLen = 0;

	while ((nIndentLen < sLead.GetLength()) &&
		   ((sLead[nIndentLen] == _T(' ')) || (sLead[nIndentLen] == _T('\t'))))
	{
		nIndentLen++;
	}

	// Enter copies only the previous line's leading whitespace.
	// Bullet markers are a wrapped-line alignment feature and are
	// deliberately not continued here.
	return sLead.Left(nIndentLen);
}

///////////////////////////////////////////////////////////////////////////

CTDLSimpleTextContentCtrl::CTDLSimpleTextContentCtrl() 
	: 
	CUrlRichEditCtrl(CTRLCLICKTOFOLLOW, IDS_CTRLCLICKTOFOLLOWLINK),
	m_bWordWrap(TRUE),
	m_bShowAllChars(FALSE),
	m_bClearUndoAfterAutoIndent(FALSE),
	m_nNumParagraphsFormatted(0)
{
	// add custom protocol to comments field for linking to task IDs
	AddProtocol(TDL_PROTOCOL, TRUE);

	EnableSelectOnFocus(FALSE);
}

CTDLSimpleTextContentCtrl::~CTDLSimpleTextContentCtrl()
{
}

BEGIN_MESSAGE_MAP(CTDLSimpleTextContentCtrl, CUrlRichEditCtrl)
	ON_WM_CONTEXTMENU()
	ON_WM_SETCURSOR()
	ON_WM_DESTROY()
	ON_WM_CREATE()
	ON_WM_HELPINFO()
	ON_WM_GETDLGCODE()
	ON_COMMAND_RANGE(ID_COMMENTS_CUT, ID_COMMENTS_LAST, OnCommentsMenuCmd)
	ON_UPDATE_COMMAND_UI_RANGE(ID_COMMENTS_CUT, ID_COMMENTS_LAST, OnUpdateCommentsMenuCmd)
	ON_CONTROL_REFLECT_EX(EN_CHANGE, OnChangeText)
	ON_CONTROL_REFLECT_EX(EN_KILLFOCUS, OnKillFocus)
	ON_MESSAGE(WM_SETWORDWRAP, OnSetWordWrap)
	ON_MESSAGE(WM_GETWORDWRAP, OnGetWordWrap)
	ON_MESSAGE(WM_SETSHOWALLCHARS, OnSetShowAllChars)
	ON_MESSAGE(WM_GETSHOWALLCHARS, OnGetShowAllChars)
	ON_WM_PAINT()
	ON_MESSAGE(WM_SETFONT, OnSetFont)
	ON_MESSAGE(WM_SETTEXT, OnSetText)
	ON_MESSAGE(WM_DROPFILES, OnDropFiles)
	ON_MESSAGE(WM_PASTE, OnPaste)
	ON_NOTIFY_REFLECT_EX(TTN_NEEDTEXT, OnGetTooltip)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// IContentCtrl

int CTDLSimpleTextContentCtrl::GetTextContent(LPTSTR szContent, int nLength) const
{
	if (!szContent)
		return GetWindowTextLength();
	
	ASSERT(nLength > 0);
	return GetWindowText(szContent, nLength);
}

bool CTDLSimpleTextContentCtrl::SetTextContent(LPCTSTR szContent, bool bResetSelection) 
{ 
	CReSaveCaret resc(bResetSelection ? NULL : GetSafeHwnd());

	SendMessage(WM_SETTEXT, 0, (LPARAM)szContent);

	// reset caret
	if (bResetSelection)
		SetSel(0, 0);

	return true; 
}

bool CTDLSimpleTextContentCtrl::InsertTextContent(LPCWSTR szContent, bool bAtEnd)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	if (!InsertSimpleText(szContent, (bAtEnd ? TRUE : FALSE)))
		return false;

	ApplyAutoIndent();
	return true;
}

int CTDLSimpleTextContentCtrl::GetContent(unsigned char* /*pContent*/) const 
{ 
	return 0; 
}

bool CTDLSimpleTextContentCtrl::SetContent(const unsigned char* /*pContent*/, int /*nLength*/, bool /*bResetSelection*/) 
{ 
	return false; 
}

void CTDLSimpleTextContentCtrl::SetReadOnly(bool bReadOnly)
{
	CUrlRichEditCtrl::SetReadOnly(bReadOnly);

	SetBackgroundColor(!bReadOnly, GetSysColor(COLOR_3DFACE));
}

void CTDLSimpleTextContentCtrl::Enable(bool bEnable)
{
	// Below Windows 8 there is an issue where moving from 
	// a disabled state causes the vertical scrollbar to not
	// update properly so we use the read-only state instead
	if (COSVersion() < OSV_WIN8)
		SetReadOnly(bEnable ? FALSE : TRUE);
	else
		EnableWindow(bEnable ? TRUE : FALSE);
}

HWND CTDLSimpleTextContentCtrl::GetHwnd() const 
{ 
	return GetSafeHwnd(); 
}

bool CTDLSimpleTextContentCtrl::DoIdleProcessing() 
{ 
	CUrlRichEditCtrl::EnableInlineSpellChecking(s_bInlineSpellChecking);

	return false; 
}

LPCTSTR CTDLSimpleTextContentCtrl::GetTypeID() const 
{ 
	return _T("PLAIN_TEXT"); 
}

bool CTDLSimpleTextContentCtrl::ProcessEditingKey(UINT nVKey, BOOL bCtrl, BOOL bAlt, BOOL bShift)
{
	switch (nVKey)
	{
	case VK_TAB:
		{
			CHARRANGE cr;
			GetSel(cr);

			const LPCTSTR CR = _T("\r");
			const CString sIndent = (s_bTabIndentWithSpaces ? _T("  ") : _T("\t"));
			const CString sCRIndent = (CString(CR) + sIndent);
			const int nIndentLen = sIndent.GetLength();

			if (bShift) // remove leading indentation
			{
				RemoveIndent();
			}
			else // insert leading indentation
			{
				CString sSel = (sIndent + GetTextRange(cr));

				// Don't include the first indent in the resulting selection
				cr.cpMin += nIndentLen;
				cr.cpMax += nIndentLen;

				// Insert an indent after every subsequent new line
				cr.cpMax += (nIndentLen * sSel.Replace(CR, sCRIndent));

				// Trim trailing newline/indent pair else next line gets indented
				// but without trimming the result because that would remove
				// any remaining indentation
				if (Misc::RemoveSuffix(sCRIndent, sSel, FALSE, FALSE))
				{
					sSel += CR; // restore carriage return
					cr.cpMax -= nIndentLen;
				}

				ReplaceSel(sSel, TRUE);
				SetSel(cr);
			}

			// Apply the new indentation immediately so the paragraph does not
			// render once with the previous hanging indent and then jump
			CHARRANGE crIndent;
			GetSel(crIndent);

			ApplyAutoIndentRange(min(crIndent.cpMin, crIndent.cpMax),
								 max(crIndent.cpMin, crIndent.cpMax), FALSE);

			return true;
		}

	case VK_RETURN:
		// auto-indent a plain <return> to match the current line
		if (!bCtrl && !bAlt && !bShift)
		{
			CHARRANGE crSel;
			GetSel(crSel);

			// Pressing Return at the start of a line must leave the
			// existing line unchanged and just insert an empty line
			CString sIndent;

			if (crSel.cpMin > FindParagraphStart(*this, crSel.cpMin))
				sIndent = GetAutoIndentPrefix(*this, crSel.cpMin);

			const CString sInsert = (_T("\r") + sIndent);

			ReplaceSel(sInsert, TRUE);

			// RichEdit leaves the caret at the line break rather than
			// after the inserted indent, so set it explicitly
			const int nCaret = (crSel.cpMin + sInsert.GetLength());

			SetSel(nCaret, nCaret);

			ApplyAutoIndentRange(min(crSel.cpMin, nCaret),
								 max(crSel.cpMin, nCaret), FALSE);

			return true;
		}
		break;
	}

	return false;
}

bool CTDLSimpleTextContentCtrl::RemoveIndent()
{
	CHARRANGE crSel;
	GetSel(crSel);

	// Read the internal text because RichEdit's character positions use
	// a single CR for each line break whereas GetWindowText returns CRLF
	CString sRaw = GetTextRange(CHARRANGE{ 0, GetTextLength() });

	if (crSel.cpMin == crSel.cpMax)
	{
		const int nLineStart = FindLineStartInText(sRaw, crSel.cpMin);
		const int nRemove = GetRemovableIndentLength(sRaw, nLineStart);

		if (nRemove > 0)
		{
			SetSel(nLineStart, (nLineStart + nRemove));
			ReplaceSel(_T(""), TRUE);

			int nCaret = crSel.cpMin;

			if (nCaret > nLineStart)
				nCaret = max(nLineStart, (nCaret - nRemove));

			SetSel(nCaret, nCaret);
		}

		return true;
	}

	// Selection: outdent every logical line touched by the selection
	const int nStart = FindLineStartInText(sRaw, crSel.cpMin);
	const int nEnd = crSel.cpMax;

	if (nEnd <= nStart)
		return true;

	const CString sRange = sRaw.Mid(nStart, (nEnd - nStart));
	CString sOut;
	int nRemoved = 0;
	BOOL bLineStart = TRUE;

	for (int nChar = 0; nChar < sRange.GetLength(); )
	{
		if (bLineStart)
		{
			const int nRemove = GetRemovableIndentLength(sRange, nChar);

			if (nRemove > 0)
			{
				nChar += nRemove;
				nRemoved += nRemove;
				bLineStart = FALSE;
				continue;
			}

			bLineStart = FALSE;
		}

		const TCHAR ch = sRange[nChar++];

		if (ch == _T('\r'))
		{
			sOut += _T('\r');

			// Store CR only; RichEdit re-expands it to CRLF
			if ((nChar < sRange.GetLength()) && (sRange[nChar] == _T('\n')))
				nChar++;

			bLineStart = TRUE;
		}
		else
		{
			sOut += ch;
		}
	}

	if (nRemoved == 0)
		return true;

	SetSel(nStart, nEnd);
	ReplaceSel(sOut, TRUE);
	SetSel(nStart, (nStart + sOut.GetLength()));

	return true;
}

bool CTDLSimpleTextContentCtrl::ProcessMessage(MSG* pMsg) 
{
	if (!IsWindowEnabled())
		return false;

	// process editing shortcuts
	if (pMsg->message == WM_KEYDOWN)
	{
		BOOL bCtrl = Misc::IsKeyPressed(VK_CONTROL);
		BOOL bAlt = Misc::IsKeyPressed(VK_MENU);
		BOOL bShift = Misc::IsKeyPressed(VK_SHIFT);

		BOOL bEnabled = !(GetStyle() & ES_READONLY);

		if (bCtrl && !bAlt)
		{
			switch (pMsg->wParam)
			{
			case 'c': 
			case 'C':
				CopySimpleText();
				return true;

			case 'v':
			case 'V':
				if (bEnabled)
				{
					Paste();
					return true;
				}
				break;

			case 'x':
			case 'X':
				CutSimpleText();
				return true;

			case 'a':
			case 'A':
				SetSel(0, -1);
				return true;

			case 'h':
			case 'H':
				if (bEnabled)
				{
					DoEditReplace(IDS_REPLACE_TITLE);
					return true;
				}
				// else fall thru

			case 'f':
			case 'F':
				DoEditFind(IDS_FIND_TITLE);
				return true;

			case 'z':
			case 'Z':
				if (bEnabled)
					return true;
				break;

			case 'y':
			case 'Y':
				if (bEnabled)
					return true;
				break;
			}
		}
		else if (bEnabled)
		{
			if (ProcessEditingKey(pMsg->wParam, bCtrl, bAlt, bShift))
				return true;
		}
	}
	else if (pMsg->message == WM_CHAR)
	{
		BOOL bEnabled = !(GetStyle() & ES_READONLY);

		if (bEnabled)
		{
			switch (pMsg->wParam)
			{
			case '@':
				// Can't handle this with WM_KEYDOWN because different
				// keyboard layouts place '@' in different locations
				{
					// Get both allocated by and allocated to
					CString sAllocTo = (LPCTSTR)GetParent()->SendMessage(WM_ICC_GETATTRIBUTELIST, TDCA_ALLOCTO, '\n');
					CString sAllocBy = (LPCTSTR)GetParent()->SendMessage(WM_ICC_GETATTRIBUTELIST, TDCA_ALLOCBY, '\n');

					CStringArray aListData, aTemp;

					Misc::Split(sAllocTo, aListData, '\n');
					Misc::Split(sAllocBy, aTemp, '\n');
					Misc::AppendItems(aTemp, aListData, TRUE);

					if (aListData.GetSize())
					{
						// Add the '@' and select it
						ReplaceSel(_T("@"), TRUE);
						SelectCharacterAtCaret(FALSE);

						return (ShowPopupListBoxAtCaret(aListData) != FALSE);
					}
				}
				break;
			}
		}
	}
	
	return false;
}

void CTDLSimpleTextContentCtrl::InitMenuIconManager()
{
	if (!m_mgrMenuIcons.Initialize(this))
		return;

	m_mgrMenuIcons.ClearImages();

	CUIntArray aCmdIDs;

	aCmdIDs.Add(ID_COMMENTS_CUT);
	aCmdIDs.Add(ID_COMMENTS_COPY);
	aCmdIDs.Add(ID_COMMENTS_PASTE);
	aCmdIDs.Add(ID_COMMENTS_DELETE);
	aCmdIDs.Add(ID_COMMENTS_WORDWRAP);
	aCmdIDs.Add(ID_COMMENTS_INLINESPELLCHECK);
	aCmdIDs.Add(ID_COMMENTS_SHOWALLCHARS);

	m_mgrMenuIcons.AddImages(aCmdIDs, IDB_SIMPLETEXT_MENU, colorMagenta);
}

void CTDLSimpleTextContentCtrl::OnSelectPopupListItem(const CString& sSelItem)
{
	if (!sSelItem.IsEmpty())
		ReplaceSel(sSelItem, TRUE);
}

void CTDLSimpleTextContentCtrl::FilterToolTipMessage(MSG* pMsg) 
{
	CUrlRichEditCtrl::FilterToolTipMessage(pMsg);
}

ISpellCheck* CTDLSimpleTextContentCtrl::GetSpellCheckInterface() 
{ 
	VERIFY(m_reSpellCheck.Initialise(*this));

	return &m_reSpellCheck; 
}

bool CTDLSimpleTextContentCtrl::Undo() 
{ 
	if (!CUrlRichEditCtrl::Undo())
		return false;

	ApplyAutoIndent();
	return true;
}

bool CTDLSimpleTextContentCtrl::Redo() 
{ 
	if (!CUrlRichEditCtrl::Redo())
		return false;

	ApplyAutoIndent();
	return true;
}

void CTDLSimpleTextContentCtrl::SetUITheme(const UITHEME* /*pTheme*/) 
{
	// Not supported
}

void CTDLSimpleTextContentCtrl::SavePreferences(IPreferences* pPrefs, LPCWSTR szKey) const
{
	pPrefs->WriteProfileInt(szKey, _T("WordWrap"), m_bWordWrap);
	pPrefs->WriteProfileInt(szKey, _T("ShowAllChars"), m_bShowAllChars);
}

void CTDLSimpleTextContentCtrl::LoadPreferences(const IPreferences* pPrefs, LPCWSTR szKey, bool bAppOnly)
{
	if (!bAppOnly)
	{
		BOOL bWordWrap = pPrefs->GetProfileInt(szKey, _T("WordWrap"), TRUE);

		// we need to post the wordwrap initialization else the richedit
		// get very confused about something and doesn't repaint properly
		// when resizing
		PostMessage(WM_SETWORDWRAP, bWordWrap, (LPARAM)GetSafeHwnd());

		PostMessage(WM_SETSHOWALLCHARS, pPrefs->GetProfileInt(szKey, _T("ShowAllChars"), FALSE), (LPARAM)GetSafeHwnd());
	}
}

bool CTDLSimpleTextContentCtrl::FindReplaceAll(LPCTSTR szFind, LPCTSTR szReplace, bool bCaseSensitive, bool bWholeWord)
{
	if (CUrlRichEditCtrl::ReplaceAll(szFind, szReplace, bCaseSensitive, bWholeWord) <= 0)
		return false;

	ApplyAutoIndent();
	return true;
}

/////////////////////////////////////////////////////////////////////////////
// CTDLSimpleTextContentCtrl message handlers

BOOL CTDLSimpleTextContentCtrl::OnHelpInfo(HELPINFO* /*lpHelpInfo*/)
{
	GetParent()->SendMessage(WM_ICC_DOHELP, 0, SIMPLETEXT_COMMENTS);
	return TRUE;
}

void CTDLSimpleTextContentCtrl::Release() 
{ 
	DestroyWindow(); 

	delete this;
}

BOOL CTDLSimpleTextContentCtrl::Create(DWORD dwStyle, const RECT& rect, CWnd* pParentWnd, UINT nID)
{
	dwStyle |= ES_NOHIDESEL | WS_HSCROLL | WS_VSCROLL | ES_WANTRETURN | ES_MULTILINE | WS_TABSTOP;

 	return CUrlRichEditCtrl::Create(dwStyle, rect, pParentWnd, nID);
}

// TODO: Check where this message is handled to make our modifications
void CTDLSimpleTextContentCtrl::SetContentFont(HFONT hFont)
{
	SendMessage(WM_SETFONT, (WPARAM)hFont);
}

BOOL CTDLSimpleTextContentCtrl::OnChangeText() 
{
	// A real edit must keep its undo history
	if (GetFocus() == this)
		m_bClearUndoAfterAutoIndent = FALSE;

	CUrlRichEditCtrl::OnChangeText();

	ApplyAutoIndentToCaretParagraph();

	// If we don't have the focus and inline spell-checking
	// is enabled then assume that this is probably not a 
	// user edit so we don't pass it on
	if ((GetFocus() != this) && IsInlineSpellCheckingEnabled())
		return TRUE;
	
	if (IsWindowEnabled() && !(GetStyle() & ES_READONLY))
		GetParent()->SendMessage(WM_ICC_CONTENTCHANGE);
	
	return FALSE;
}

BOOL CTDLSimpleTextContentCtrl::OnKillFocus() 
{
	GetParent()->SendMessage(WM_ICC_KILLFOCUS);
	
	return FALSE; // continue routing
}

LRESULT CTDLSimpleTextContentCtrl::OnSetFont(WPARAM wp, LPARAM lp) 
{
	LRESULT lr = CUrlRichEditCtrl::OnSetFont(wp, lp);

	// A programmatic font change can reformat links, so do not leave
	// that formatting in the undo history
	if (GetFocus() != this)
		m_bClearUndoAfterAutoIndent = TRUE;

	ApplyAutoIndent();

	return lr;
}

LRESULT CTDLSimpleTextContentCtrl::OnSetText(WPARAM wp, LPARAM lp) 
{
	LRESULT lr = CUrlRichEditCtrl::OnSetText(wp, lp);

	// Only an actual content change invalidates the undo buffer
	if (lr)
	{
		m_bClearUndoAfterAutoIndent = TRUE;
		ApplyAutoIndent();
	}

	return lr;
}

LRESULT CTDLSimpleTextContentCtrl::OnDropFiles(WPARAM wp, LPARAM lp)
{
	LRESULT lr = CUrlRichEditCtrl::OnDropFiles(wp, lp);

	ApplyAutoIndent();
	return lr;
}

LRESULT CTDLSimpleTextContentCtrl::OnPaste(WPARAM /*wp*/, LPARAM /*lp*/)
{
	LRESULT lr = Default(); // let the rich edit paste

	ApplyAutoIndent();
	return lr;
}

HRESULT CTDLSimpleTextContentCtrl::QueryAcceptData(LPDATAOBJECT lpdataobj, CLIPFORMAT FAR* lpcfFormat, 
													DWORD reco, BOOL fReally, HGLOBAL hMetaPict)
{
	HRESULT hr = CUrlRichEditCtrl::QueryAcceptData(lpdataobj, lpcfFormat, reco, fReally, hMetaPict);

	if (fReally && SUCCEEDED(hr))
		ApplyAutoIndent();

	return hr;
}

void CTDLSimpleTextContentCtrl::ApplyAutoIndent()
{
	ApplyAutoIndentRange(0, GetTextLength(), TRUE);
}

void CTDLSimpleTextContentCtrl::ApplyAutoIndentToCaretParagraph()
{
	CHARRANGE crSel;
	GetSel(crSel);

	ApplyAutoIndentRange(crSel.cpMin, crSel.cpMax, FALSE);
}

void CTDLSimpleTextContentCtrl::ApplyAutoIndentRange(int nFrom, int nTo, BOOL bFullPass)
{
	m_nNumParagraphsFormatted = 0;

	const int nTextLen = GetTextLength();

	if ((nTextLen <= 0) || (nTextLen > MAX_AUTOINDENT_LENGTH))
	{
		if (m_bClearUndoAfterAutoIndent)
		{
			EmptyUndoBuffer();
			m_bClearUndoAfterAutoIndent = FALSE;
		}

		return;
	}

	CHARRANGE crSel;
	GetSel(crSel);

	const int nFirstLine = GetFirstVisibleLine();
	const BOOL bVisible = IsWindowVisible();
	const BOOL bCaret = (bFullPass && (GetFocus() == this));

	if (bCaret)
		HideCaret();

	CRePauseUndo rep(*this);
	DWORD dwEventMask = SetEventMask(0);

	if (bFullPass && bVisible)
		SetRedraw(FALSE);

	CClientDC dc(this);
	const int nDpiX = dc.GetDeviceCaps(LOGPIXELSX);

	const int nStart = (bFullPass ? 0 : max(0, min(nFrom, nTextLen)));
	const int nEnd = (bFullPass ? nTextLen : max(nStart, min(nTo, nTextLen)));
	int nParaStart = FindParagraphStart(*this, nStart);

	for (;;)
	{
		int nParaEnd = FindParagraphEnd(*this, nParaStart, nTextLen);

		ApplyAutoIndentToParagraph(nParaStart, nParaEnd, nDpiX);

		if ((nParaEnd >= nTextLen) || (!bFullPass && (nParaEnd >= nEnd)))
			break;

		nParaStart = (nParaEnd + 1);
	}

	CHARRANGE crNow;
	GetSel(crNow);

	if ((crNow.cpMin != crSel.cpMin) || (crNow.cpMax != crSel.cpMax))
		SetSel(crSel);

	if (GetFirstVisibleLine() != nFirstLine)
		SetFirstVisibleLine(nFirstLine);

	SetEventMask(dwEventMask);

	if (bFullPass)
	{
		if (bVisible)
		{
			SetRedraw(TRUE);

			// Replacing the content can leave the old text visible beyond the
			// new text, so always repaint the whole control after a full pass
			Invalidate(FALSE);
		}

		if (bCaret)
			ShowCaret();
	}

	// Auto-indent is formatting-only, not a user edit
	if (m_bClearUndoAfterAutoIndent)
	{
		EmptyUndoBuffer();
		m_bClearUndoAfterAutoIndent = FALSE;
	}
}

bool CTDLSimpleTextContentCtrl::ApplyAutoIndentToParagraph(int nParaStart, int nParaEnd, int nDpiX)
{
	m_nNumParagraphsFormatted++;

	CHARRANGE crPara = { nParaStart, nParaEnd };
	CString sPara = GetTextRange(crPara);

	int nFirstNonWs, nBulletLen, nTextStart;
	GetParagraphPrefix(sPara, nFirstNonWs, nBulletLen, nTextStart);

	int nOffset = 0;

	// Avoid moving the selection when it is already inside this paragraph;
	// moving it to a long paragraph's start would scroll the view and force
	// unrelated text to repaint on every keystroke.
	CHARRANGE crSel;
	GetSel(crSel);

	const BOOL bSelInPara = ((crSel.cpMin >= nParaStart) && (crSel.cpMin <= nParaEnd));

	if (!bSelInPara)
		SetSel(nParaStart, nParaStart);

	PARAFORMAT2 pf;
	ZeroMemory(&pf, sizeof(pf));
	pf.cbSize = sizeof(pf);

	GetParaFormat(pf);

	// RichEdit lays out a first-line tab against the hanging-indent
	// position (dxStartIndent + dxOffset) instead of the line's actual
	// left edge. The character after the tab therefore lands on the
	// continuation indent, and the text after the marker stays
	// marker-width to its right. Changing dxOffset moves both the
	// continuation and the tab target by the same amount, so no offset
	// can align a tab-prefixed paragraph. Only align space prefixes.
	if ((nTextStart > 0) && (nTextStart < sPara.GetLength()) &&
		(sPara.Left(nTextStart).Find(_T('\t')) < 0))
	{
		CPoint ptPara, ptText;
		PointFromChar(nParaStart, ptPara);
		PointFromChar((nParaStart + nTextStart), ptText);

		// PointFromChar returns pixels for RichEdit 2.0 and later
		if ((ptText.y == ptPara.y) && (ptText.x > ptPara.x))
			nOffset = MulDiv((ptText.x - ptPara.x), 1440, nDpiX);
	}

	if ((pf.dwMask & PFM_OFFSET) && (pf.dxOffset == nOffset))
		return false;

	PARAFORMAT2 pfNew;
	ZeroMemory(&pfNew, sizeof(pfNew));
	pfNew.cbSize = sizeof(pfNew);
	pfNew.dwMask = PFM_OFFSET;
	pfNew.dxOffset = nOffset;

	::SendMessage(GetSafeHwnd(), EM_SETPARAFORMAT, 0, (LPARAM)&pfNew);

	return true;
}

void CTDLSimpleTextContentCtrl::OnContextMenu(CWnd* pWnd, CPoint point) 
{
	if (WantIgnoreContextMenu())
		return;

	if (pWnd == this)
	{
		// build the context menu ourselves
		// note: we could use the edit menu from user32.dll but because
		// we're adding items too, the languages would appear odd
		CEnMenu menu;
		
		if (menu.LoadMenu(IDR_MISC, *this, TRUE))
		{
			CMenu* pPopup = menu.GetSubMenu(MM_COMMENTS);

			if (pPopup)
			{
#ifndef _DEBUG
				if (!SupportsInlineSpellChecking())
					pPopup->DeleteMenu(ID_COMMENTS_INLINESPELLCHECK, MF_BYCOMMAND);
#endif
				// Remove URL commands if URL not clicked on
				if (m_sContextUrl.IsEmpty())
				{
					pPopup->DeleteMenu(ID_COMMENTS_OPENURL, MF_BYCOMMAND);
					pPopup->DeleteMenu(ID_COMMENTS_COPYURL, MF_BYCOMMAND);
				}
				
				CToolbarHelper::PrepareMenuItems(pPopup, this);
				pPopup->TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON, point.x, point.y, this);
			}
		}
	}
}

void CTDLSimpleTextContentCtrl::OnCommentsMenuCmd(UINT nCmdID) 
{
	switch (nCmdID)
	{
	case ID_COMMENTS_UNDO:
		if (CanEdit())
			CTextDocument(GetSafeHwnd()).Undo();
		break;

	case ID_COMMENTS_REDO:
		if (CanEdit())
			CTextDocument(GetSafeHwnd()).Redo();
		break;

	case ID_COMMENTS_CUT:
		CutSimpleText();
		break;

	case ID_COMMENTS_DELETE:
		if (CanEdit())
			ReplaceSel(_T(""));
		break;

	case ID_COMMENTS_FIND:
		DoEditFind(IDS_FIND_TITLE);
		break;
		
	case ID_COMMENTS_FINDREPLACE:
		DoEditReplace(IDS_REPLACE_TITLE);
		break;
		
	case ID_COMMENTS_COPY:
		CopySimpleText();
		break;
		
	case ID_COMMENTS_PASTE:
		Paste();
		break;
		
	case ID_COMMENTS_PASTEASREF:
		if (CanEdit())
		{
			// try to get the clipboard for any tasklist
			ITaskList* pClipboard = (ITaskList*)GetParent()->SendMessage(WM_ICC_GETCLIPBOARD, 0, FALSE);

			// verify that we can get the corresponding filename
			CString sFileName;
			ITaskList17* pClip17 = GetITLInterface<ITaskList17>(pClipboard, IID_TASKLIST17);

			if (pClip17)
			{
				sFileName = pClip17->GetFileName(false);
				sFileName.Replace(_T(" "), _T("%20"));
			}
			else // get the clipboard for just this tasklist
			{
				pClipboard = (ITaskList*)GetParent()->SendMessage(WM_ICC_GETCLIPBOARD, 0, TRUE);
			}

			if (pClipboard && pClipboard->GetFirstTask())
			{
				// build single string containing each top level item as a different ref
				CString sRefs, sRef;
				HTASKITEM hClip = pClipboard->GetFirstTask();
				
				while (hClip)
				{
					if (sFileName.IsEmpty())
						sRef.Format(_T(" %s%lu"), TDL_PROTOCOL, pClipboard->GetTaskID(hClip));
					else
						sRef.Format(_T(" %s%s?%lu"), TDL_PROTOCOL, sFileName, pClipboard->GetTaskID(hClip));

					sRefs += sRef;
					
					hClip = pClipboard->GetNextTask(hClip);
				}

				sRefs += ' ';
				ReplaceSel(sRefs, TRUE);
			}
		}
		break;
		
	case ID_COMMENTS_SELECTALL:
		SetSel(0, -1);
		break;
		
	case ID_COMMENTS_OPENURL:
		if (!m_sContextUrl.IsEmpty())
			GoToUrl(m_sContextUrl);
		break;
		
	case ID_COMMENTS_COPYURL:
		if (!m_sContextUrl.IsEmpty())
			CClipboard(*this).SetText(m_sContextUrl);
		break;
		
	case ID_COMMENTS_FILEBROWSE:
		if (CanEdit())
		{
			CString sFile = CUrlParser::GetUrlAsFile(m_sContextUrl);
						
			CFileOpenDialog dialog(IDS_BROWSEFILE_TITLE, NULL, sFile, (EOFN_DEFAULTOPEN | OFN_ALLOWMULTISELECT));
			
			if (dialog.DoModal() == IDOK)
			{
				CStringArray aPaths;
				
				if (dialog.GetPathNames(aPaths) > 0)
				{
					SetFocus();
					CRichEditHelper::PasteFiles(*this, aPaths, REP_ASFILEURL, FALSE);
				}
			}
		}
		break;
		
	case ID_COMMENTS_SPELLCHECK:
		if (CanEdit())
			GetParent()->PostMessage(WM_ICC_WANTSPELLCHECK);
		break;

	case ID_COMMENTS_WORDWRAP:
		SetWordWrap(!m_bWordWrap);
		break;

	case ID_COMMENTS_SHOWALLCHARS:
		SetShowAllChars(!m_bShowAllChars);
		break;

	case ID_COMMENTS_INLINESPELLCHECK:
#ifndef _DEBUG
		if (SupportsInlineSpellChecking())
#endif
		{
			s_bInlineSpellChecking = !s_bInlineSpellChecking;
			CUrlRichEditCtrl::EnableInlineSpellChecking(s_bInlineSpellChecking);
		}
		break;
	}
}

void CTDLSimpleTextContentCtrl::SetWordWrap(BOOL bWrap)
{
	SetTargetDevice(NULL, bWrap ? 0 : 1);
	m_bWordWrap = bWrap;

	Invalidate(FALSE);
}

void CTDLSimpleTextContentCtrl::SetShowAllChars(BOOL bShow)
{
	if (m_bShowAllChars != bShow)
	{
		m_bShowAllChars = bShow;
		Invalidate(FALSE);
	}
}

void CTDLSimpleTextContentCtrl::OnUpdateCommentsMenuCmd(CCmdUI* pCmdUI)
{
	switch (pCmdUI->m_nID)
	{
	case ID_COMMENTS_UNDO:
		pCmdUI->Enable(CanEdit() && CanUndo());
		break;
					
	case ID_COMMENTS_REDO:
		pCmdUI->Enable(CanEdit() && SendMessage(EM_CANREDO));
		break;
					
	case ID_COMMENTS_CUT:
		pCmdUI->Enable(CanCutSelectedText());
		break;
		
	case ID_COMMENTS_COPY:
		pCmdUI->Enable(CanCopySelectedText());
		break;
		
	case ID_COMMENTS_PASTE:
		pCmdUI->Enable(CanPaste());
		break;
		
	case ID_COMMENTS_DELETE:
		pCmdUI->Enable(CanEdit());
		break;
		
	case ID_COMMENTS_PASTEASREF:
		pCmdUI->Enable(CanEdit() && !IsTDLClipboardEmpty());
		break;

	case ID_COMMENTS_FIND:
		pCmdUI->Enable(TRUE);
		break;
		
	case ID_COMMENTS_FINDREPLACE:
		pCmdUI->Enable(CanEdit());
		break;
		
	case ID_COMMENTS_SELECTALL:
		pCmdUI->Enable(GetTextLength());
		break;
		
	case ID_COMMENTS_SPELLCHECK:
		pCmdUI->Enable(GetTextLength() && CanEdit());
		break;
		
	case ID_COMMENTS_OPENURL:
	case ID_COMMENTS_COPYURL: 
		// if pCmdUI->m_pMenu is NOT null then we need to set the menu
		// text to the url, else it's just MFC checking that the option
		// is still enabled
		pCmdUI->Enable(!m_sContextUrl.IsEmpty());

		if (!m_sContextUrl.IsEmpty() && pCmdUI->m_pMenu)
		{
			CEnString sUrl(CUrlParser::GetUrlAsFile(m_sContextUrl));

			// restrict url length to 250 pixels
			CClientDC dc(this);
			sUrl.FormatDC(&dc, 250, ES_PATH);

			CString sText, sMenu;
			pCmdUI->m_pMenu->GetMenuString(pCmdUI->m_nID, sMenu, MF_BYCOMMAND);

			sText.Format(_T("%s: %s"), CEnString(sMenu), sUrl);
			pCmdUI->SetText(sText);
		}
		break;
		
	case ID_COMMENTS_FILEBROWSE:
		pCmdUI->Enable(CanEdit());
		break;
		
	case ID_COMMENTS_WORDWRAP:
		pCmdUI->SetCheck(m_bWordWrap ? 1 : 0);
		break;

	case ID_COMMENTS_SHOWALLCHARS:
		pCmdUI->SetCheck(m_bShowAllChars ? 1 : 0);
		break;

	case ID_COMMENTS_INLINESPELLCHECK:
#ifndef _DEBUG
		if (SupportsInlineSpellChecking())
		{
			pCmdUI->Enable(TRUE);
			pCmdUI->SetCheck(s_bInlineSpellChecking);
		}
		else
		{
			pCmdUI->SetCheck(FALSE);
			pCmdUI->Enable(FALSE);
		}
#else
		pCmdUI->Enable(TRUE);
		pCmdUI->SetCheck(s_bInlineSpellChecking);
#endif
		break;
	}
}

BOOL CTDLSimpleTextContentCtrl::Paste()
{
	if (!CanEdit())
		return FALSE;
	
	CStringArray aFiles;
	int nNumFiles = CClipboard().GetDropFilePaths(aFiles);
	BOOL bPasted = FALSE;

	switch (nNumFiles)
	{
	case -1:
		AfxMessageBox(CEnString(IDS_PASTE_ERROR), MB_OK | MB_ICONERROR);
		return FALSE;

	case 0:
		bPasted = PasteSimpleText(s_bPasteSourceUrls);
		break;

	default:
		bPasted = CRichEditHelper::PasteFiles(*this, aFiles, REP_ASFILEURL, FALSE);
		break;
	}

	if (bPasted)
		ApplyAutoIndent();

	return bPasted;
}

BOOL CTDLSimpleTextContentCtrl::CanPaste()
{
	if (!CanEdit())
		return FALSE;

	// for reasons that I'm not entirely clear on even if we 
	// return that CF_HDROP is okay, the richedit itself will
	// veto the drop. So I'm experimenting with handling this ourselves
	if (CClipboard().HasFormat(CF_HDROP))
		return TRUE;

	// else
	return CanPasteSimpleText();
}


BOOL CTDLSimpleTextContentCtrl::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message) 
{
	// bit of a hack but this is what we get just before the context
	// menu appears so we set the cursor back to the arrow
	if (nHitTest == HTCAPTION)
	{
		SetCursor(AfxGetApp()->LoadStandardCursor(IDC_ARROW));
		return TRUE;
	}
	
	return CUrlRichEditCtrl::OnSetCursor(pWnd, nHitTest, message);
}

BOOL CTDLSimpleTextContentCtrl::IsTDLClipboardEmpty() const 
{ 
	// try for any clipboard first
	ITaskList* pClipboard = (ITaskList*)GetParent()->SendMessage(WM_ICC_GETCLIPBOARD, 0, FALSE);
	ITaskList4* pClip4 = GetITLInterface<ITaskList4>(pClipboard, IID_TASKLIST4);

	if (pClip4)
		return (pClipboard->GetFirstTask() == NULL);

	// else try for 'our' clipboard only
	return (!GetParent()->SendMessage(WM_ICC_HASCLIPBOARD, 0, TRUE)); 
}

LRESULT CTDLSimpleTextContentCtrl::SendNotifyCustomUrl(LPCTSTR szUrl) const
{
	CString sUrl(szUrl);
	sUrl.MakeLower();

	if (sUrl.Find(TDL_PROTOCOL) != -1 || sUrl.Find(TDL_EXTENSION) != -1)
	{
		return GetParent()->SendMessage(WM_ICC_TASKLINK, 0, (LPARAM)(LPCTSTR)sUrl);
	}

	return SendNotifyFailedUrl(szUrl);
}

LRESULT CTDLSimpleTextContentCtrl::SendNotifyFailedUrl(LPCTSTR szUrl) const
{
	return GetParent()->SendMessage(WM_ICC_FAILEDLINK, (WPARAM)GetSafeHwnd(), (LPARAM)szUrl);
}

void CTDLSimpleTextContentCtrl::PreSubclassWindow() 
{
	CUrlRichEditCtrl::PreSubclassWindow();
}

LRESULT CTDLSimpleTextContentCtrl::OnSetWordWrap(WPARAM wp, LPARAM lp)
{
	UNREFERENCED_PARAMETER (lp);
	ASSERT (lp == (LPARAM)GetSafeHwnd());
	
	SetWordWrap(wp);
	return 0L;
}

LRESULT CTDLSimpleTextContentCtrl::OnGetWordWrap(WPARAM /*wp*/, LPARAM /*lp*/)
{
	return (m_bWordWrap ? 1L : 0L);
}

LRESULT CTDLSimpleTextContentCtrl::OnSetShowAllChars(WPARAM wp, LPARAM /*lp*/)
{
	SetShowAllChars(wp);
	return 0L;
}

LRESULT CTDLSimpleTextContentCtrl::OnGetShowAllChars(WPARAM /*wp*/, LPARAM /*lp*/)
{
	return (m_bShowAllChars ? 1L : 0L);
}

void CTDLSimpleTextContentCtrl::OnPaint()
{
	// Let the rich edit draw its content first
	Default();

	if (m_bShowAllChars)
	{
		CClientDC dc(this);
		DrawWhitespace(dc);
	}
}

void CTDLSimpleTextContentCtrl::DrawWhitespace(CDC& dc) const
{
	const int nTextLen = GetTextLength();

	if (nTextLen <= 0)
		return;

	CRect rClient, rText;
	GetClientRect(rClient);
	GetRect(&rText);
	rText.IntersectRect(rText, rClient);

	if (rText.IsRectEmpty())
		return;

	POINTL ptFirst = { (rText.left + 1), (rText.top + 1) };
	POINTL ptLast = { (rText.right - 1), (rText.bottom - 1) };

	int nFirst = (int)SendMessage(EM_CHARFROMPOS, 0, (LPARAM)&ptFirst);
	int nLast = (int)SendMessage(EM_CHARFROMPOS, 0, (LPARAM)&ptLast);

	nFirst = max(0, nFirst);
	nLast = min((nTextLen - 1), nLast);

	if (nLast < nFirst)
		return;

	CString sVisible = GetTextRange(CHARRANGE{ nFirst, (nLast + 1) });
	const int nLineHeight = GetLineHeight();

	const COLORREF crMarker = RGB(140, 140, 140);
	CPen pen(PS_SOLID, 1, crMarker);
	CBrush brush(crMarker);

	CPen* pOldPen = dc.SelectObject(&pen);
	CBrush* pOldBrush = dc.SelectObject(&brush);
	const int nOldBkMode = dc.SetBkMode(TRANSPARENT);

	for (int nChar = 0; nChar < sVisible.GetLength(); nChar++)
	{
		const TCHAR ch = sVisible[nChar];

		if ((ch != _T(' ')) && (ch != _T('\t')))
			continue;

		const int nPos = (nFirst + nChar);

		CPoint ptChar, ptNext;
		PointFromChar(nPos, ptChar);
		PointFromChar((nPos + 1), ptNext);

		if ((ptChar.y < rText.top) || (ptChar.y >= rText.bottom) ||
			(ptChar.x < rText.left) || (ptChar.x >= rText.right))
		{
			continue;
		}

		const int nMarkerY = (ptChar.y + ((nLineHeight * 3) / 4));

		if (ch == _T(' '))
		{
			int nCellWidth = ((ptNext.y == ptChar.y) ? (ptNext.x - ptChar.x) : (nLineHeight / 2));
			nCellWidth = max(nCellWidth, 2);

			const int nDotX = (ptChar.x + (nCellWidth / 2));
			dc.Ellipse((nDotX - 1), (nMarkerY - 1), (nDotX + 1), (nMarkerY + 1));
		}
		else // tab
		{
			int nTabEnd = ((ptNext.y == ptChar.y) ? ptNext.x : rText.right);
			nTabEnd = min(nTabEnd, rText.right);

			if ((nTabEnd - ptChar.x) > 4)
			{
				const int nArrowEnd = (nTabEnd - 2);

				dc.MoveTo((ptChar.x + 2), nMarkerY);
				dc.LineTo(nArrowEnd, nMarkerY);
				dc.MoveTo((nArrowEnd - 3), (nMarkerY - 3));
				dc.LineTo(nArrowEnd, nMarkerY);
				dc.MoveTo((nArrowEnd - 3), (nMarkerY + 3));
				dc.LineTo(nArrowEnd, nMarkerY);
			}
		}
	}

	dc.SelectObject(pOldBrush);
	dc.SelectObject(pOldPen);
	dc.SetBkMode(nOldBkMode);
}

int CTDLSimpleTextContentCtrl::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (CUrlRichEditCtrl::OnCreate(lpCreateStruct) == -1)
		return -1;

	LimitText(1024 * 1024 * 1024); // one gigabyte
	EnableToolTips();
	InitMenuIconManager();

	CUrlRichEditCtrl::EnableInlineSpellChecking(s_bInlineSpellChecking);
	
	return 0;
}

BOOL CTDLSimpleTextContentCtrl::OnGetTooltip(NMHDR* pNMHDR, LRESULT* pResult)
{
	ASSERT(pNMHDR->idFrom == (UINT)GetDlgCtrlID());
	ASSERT(pNMHDR->hwndFrom == GetSafeHwnd());

	TOOLTIPTEXT* pTTN = (TOOLTIPTEXT*)pNMHDR;
	*pResult = 0;

	LPCTSTR szTooltip = (LPCTSTR)GetParent()->SendMessage(WM_ICC_GETLINKTOOLTIP, (WPARAM)GetSafeHwnd(), (LPARAM)pTTN->lpszText);

	if (!Misc::IsEmpty(szTooltip))
	{
		lstrcpyn(pTTN->szText, szTooltip, (sizeof(pTTN->szText) / sizeof(pTTN->szText[0])));
		*pResult = TRUE;

		return TRUE;
	}

	return FALSE;
}

CLIPFORMAT CTDLSimpleTextContentCtrl::GetAcceptableClipFormat(LPDATAOBJECT lpDataOb, CLIPFORMAT format)
{ 
	// check for outlook first
	if (CMSOutlookHelper::IsOutlookObject(lpDataOb))
		return CMSOutlookHelper::CF_OUTLOOK;

	// FALSE ensures we only get what we allow
	return CRichEditHelper::GetAcceptableClipFormat(lpDataOb, format, CF_PREFERRED, NUM_PREF, FALSE);
}

UINT CTDLSimpleTextContentCtrl::OnGetDlgCode()
{
	return (DLGC_WANTALLKEYS & ~DLGC_HASSETSEL);
}

