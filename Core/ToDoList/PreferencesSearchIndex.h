#if !defined(AFX_PREFERENCESSEARCHINDEX_H__A0D7E4C2_7B3F_4C61_9E2A_5D8F1C3B6E47__INCLUDED_)
#define AFX_PREFERENCESSEARCHINDEX_H__A0D7E4C2_7B3F_4C61_9E2A_5D8F1C3B6E47__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif

#include <afxtempl.h>

/////////////////////////////////////////////////////////////////////////////

class CPreferencesSearchIndex
{
public:
	CPreferencesSearchIndex();
	virtual ~CPreferencesSearchIndex();

	void Clear();
	BOOL IsEmpty() const { return (m_aPageText.GetSize() == 0); }

	void SetPageCount(int nPages);
	int GetPageCount() const { return m_aPageText.GetSize(); }

	void AddText(int nPage, const CString& sText);
	void AddText(int nPage, const CStringArray& aText);
	void SetAlwaysMatches(int nPage, BOOL bAlways = TRUE);

	BOOL AddDialogTemplate(int nPage, LPCTSTR szTemplate);

	BOOL PageMatches(int nPage, const CStringArray& aSearch) const;

	const CStringArray& GetPageText(int nPage) const;

	static BOOL ParseDialogTemplate(const DLGTEMPLATE* pTemplate, CStringArray& aText);

protected:
	CArray<CStringArray*, CStringArray*> m_aPageText;
	CUIntArray m_aAlwaysMatch;

protected:
	void EnsurePage(int nPage);
};

/////////////////////////////////////////////////////////////////////////////

#endif // !defined(AFX_PREFERENCESSEARCHINDEX_H__A0D7E4C2_7B3F_4C61_9E2A_5D8F1C3B6E47__INCLUDED_)
