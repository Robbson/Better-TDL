// PreferencesSearchIndex.cpp : implementation file
//

#include "stdafx.h"
#include "PreferencesSearchIndex.h"

#include "..\shared\AcceleratorString.h"
#include "..\shared\CtrlTextHighlighter.h"
#include "..\shared\Localizer.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////

namespace
{
	const BYTE* AlignToDWord(const BYTE* p)
	{
		UINT_PTR nPtr = reinterpret_cast<UINT_PTR>(p);

		return reinterpret_cast<const BYTE*>((nPtr + 3) & ~static_cast<UINT_PTR>(3));
	}

	WORD ReadWORD(const BYTE*& p)
	{
		WORD wValue = 0;
		memcpy(&wValue, p, sizeof(wValue));
		p += sizeof(wValue);

		return wValue;
	}

	const BYTE* SkipTemplateString(const BYTE* p, BOOL bUnicode, CString* pText = NULL)
	{
		WORD wFirst = 0;
		memcpy(&wFirst, p, sizeof(wFirst));

		if (wFirst == 0x0000)
		{
			return (p + sizeof(WORD));
		}
		else if (wFirst == 0xFFFF)
		{
			if (pText)
			{
				WORD wOrdinal = 0;
				memcpy(&wOrdinal, (p + sizeof(WORD)), sizeof(wOrdinal));
				pText->Format(_T("#%u"), wOrdinal);
			}

			return (p + (2 * sizeof(WORD)));
		}
		else if (bUnicode)
		{
			LPCWSTR szText = reinterpret_cast<LPCWSTR>(p);

			if (pText)
				*pText = szText;

			return (p + ((wcslen(szText) + 1) * sizeof(WCHAR)));
		}
		else
		{
			LPCSTR szText = reinterpret_cast<LPCSTR>(p);

			if (pText)
				*pText = szText;

			return (p + (strlen(szText) + 1));
		}
	}
}

/////////////////////////////////////////////////////////////////////////////

CPreferencesSearchIndex::CPreferencesSearchIndex()
{
}

CPreferencesSearchIndex::~CPreferencesSearchIndex()
{
	Clear();
}

void CPreferencesSearchIndex::Clear()
{
	for (int nPage = 0; nPage < m_aPageText.GetSize(); nPage++)
		delete m_aPageText[nPage];

	m_aPageText.RemoveAll();
	m_aAlwaysMatch.RemoveAll();
}

void CPreferencesSearchIndex::EnsurePage(int nPage)
{
	ASSERT(nPage >= 0);

	while (m_aPageText.GetSize() <= nPage)
		m_aPageText.Add(new CStringArray());

	while (m_aAlwaysMatch.GetSize() <= nPage)
		m_aAlwaysMatch.Add(0);
}

void CPreferencesSearchIndex::SetPageCount(int nPages)
{
	if (nPages > 0)
		EnsurePage(nPages - 1);
}

void CPreferencesSearchIndex::AddText(int nPage, const CString& sText)
{
	if (sText.IsEmpty())
		return;

	EnsurePage(nPage);

	m_aPageText[nPage]->Add(sText);
}

void CPreferencesSearchIndex::AddText(int nPage, const CStringArray& aText)
{
	for (int nText = 0; nText < aText.GetSize(); nText++)
		AddText(nPage, aText[nText]);
}

void CPreferencesSearchIndex::SetAlwaysMatches(int nPage, BOOL bAlways)
{
	EnsurePage(nPage);

	m_aAlwaysMatch[nPage] = (bAlways ? 1 : 0);
}

BOOL CPreferencesSearchIndex::AddDialogTemplate(int nPage, LPCTSTR szTemplate)
{
	HINSTANCE hInst = AfxFindResourceHandle(szTemplate, RT_DIALOG);
	HRSRC hRes = ::FindResource(hInst, szTemplate, RT_DIALOG);

	if (!hRes)
		return FALSE;

	HGLOBAL hMem = ::LoadResource(hInst, hRes);

	if (!hMem)
		return FALSE;

	const DLGTEMPLATE* pTemplate = reinterpret_cast<const DLGTEMPLATE*>(::LockResource(hMem));

	if (!pTemplate)
		return FALSE;

	CStringArray aText;

	if (!ParseDialogTemplate(pTemplate, aText))
		return FALSE;

	for (int nText = 0; nText < aText.GetSize(); nText++)
	{
		CString sText = CLocalizer::TranslateText((LPCTSTR)aText[nText]);

		CAcceleratorString::RemoveAccelerator(sText);
		AddText(nPage, sText);
	}

	return TRUE;
}

BOOL CPreferencesSearchIndex::ParseDialogTemplate(const DLGTEMPLATE* pTemplate, CStringArray& aText)
{
	if (!pTemplate)
		return FALSE;

	const BYTE* p = reinterpret_cast<const BYTE*>(pTemplate);
	WORD wDlgVer = 0, wSignature = 0;
	memcpy(&wDlgVer, p, sizeof(wDlgVer));
	memcpy(&wSignature, (p + sizeof(WORD)), sizeof(wSignature));

	const BOOL bExtended = ((wDlgVer == 0x0001) && (wSignature == 0xFFFF));
#ifdef _UNICODE
	const BOOL bUnicodeStrings = TRUE;
#else
	const BOOL bUnicodeStrings = FALSE;
#endif

	DWORD dwStyle = 0;
	WORD wNumItems = 0;

	if (bExtended)
	{
		p += (3 * sizeof(DWORD)); // dlgVer/signature + helpID + exStyle
		p += sizeof(DWORD); // style
		wNumItems = ReadWORD(p);
		p += (4 * sizeof(short)); // x, y, cx, cy
	}
	else
	{
		memcpy(&dwStyle, p, sizeof(dwStyle));
		p += (2 * sizeof(DWORD)); // style + exStyle
		wNumItems = ReadWORD(p);
		p += (4 * sizeof(short)); // x, y, cx, cy
	}

	// menu, class and caption
	p = SkipTemplateString(p, bUnicodeStrings);
	p = SkipTemplateString(p, bUnicodeStrings);

	CString sCaption;
	p = SkipTemplateString(p, bUnicodeStrings, &sCaption);

	if (!sCaption.IsEmpty())
		aText.Add(sCaption);

	// font information
	if (bExtended)
	{
		p += sizeof(WORD); // point size
		p += sizeof(WORD); // weight
		p += sizeof(BYTE); // italic
		p += sizeof(BYTE); // charset
		p = SkipTemplateString(p, bUnicodeStrings); // typeface
	}
	else if (dwStyle & DS_SETFONT)
	{
		p += sizeof(WORD); // point size
		p = SkipTemplateString(p, bUnicodeStrings); // typeface
	}

	for (WORD wItem = 0; wItem < wNumItems; wItem++)
	{
		p = AlignToDWord(p);

		if (bExtended)
		{
			p += (3 * sizeof(DWORD)); // helpID + exStyle + style
			p += (4 * sizeof(short)); // x, y, cx, cy
			p += sizeof(DWORD); // id
		}
		else
		{
			p += (2 * sizeof(DWORD)); // style + exStyle
			p += (4 * sizeof(short)); // x, y, cx, cy
			p += sizeof(WORD); // id
		}

		p = SkipTemplateString(p, bUnicodeStrings); // class

		CString sTitle;
		p = SkipTemplateString(p, bUnicodeStrings, &sTitle);

		if (!sTitle.IsEmpty())
			aText.Add(sTitle);

		WORD wExtraData = ReadWORD(p);
		p += wExtraData;
	}

	return TRUE;
}

BOOL CPreferencesSearchIndex::PageMatches(int nPage, const CStringArray& aSearch) const
{
	if ((nPage < 0) || (nPage >= m_aPageText.GetSize()))
		return FALSE;

	ASSERT(nPage < m_aAlwaysMatch.GetSize());

	if (m_aAlwaysMatch[nPage])
		return TRUE;

	return CCtrlTextHighlighter::TextContainsOneOf(*m_aPageText[nPage], aSearch);
}

const CStringArray& CPreferencesSearchIndex::GetPageText(int nPage) const
{
	ASSERT((nPage >= 0) && (nPage < m_aPageText.GetSize()));

	return *m_aPageText[nPage];
}
