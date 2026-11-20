/* ==========================================================================
	File :			FontComboBox.cpp

	Class :			CFontComboBox

	Author :		Johan Rosengren, Abstrakt Mekanik AB
					Iain Clarke

	Date :			2005-05-06

	Purpose :		CFontComboBox is derived from "CComboBox" and is an 
					autosizing no-frills combobox for display of the fonts 
					installed in the system.. 

	Description :	Simpel derived class with members to fill the box, to 
					autosize the dropdown and select an entry by name.

	Usage :			Create as any combobox, and call "FillCombo" to fill 
					the control with the names of the fonts installed in 
					the system. Call "SelectFontName" to select a font by 
					name.

   ========================================================================*/

#include "stdafx.h"
#include "FontComboBox.h"

#include "..\shared\ScopedTimer.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// Enumeration callback function for 
// installed fonts

namespace
{
	struct FONTNAMECACHE
	{
		FONTNAMECACHE() : nMaxWidth(0), bValid(FALSE) {}

		CStringArray aNames;
		CMap<CString, LPCTSTR, BOOL, BOOL> mapSeen;
		int nMaxWidth;
		BOOL bValid;
	};

	FONTNAMECACHE& GetFontNameCache()
	{
		static FONTNAMECACHE cache;
		return cache;
	}

	int CALLBACK EnumFontFamilyProc(const LOGFONT* lplf, const TEXTMETRIC* /*lptm*/, DWORD /*dwType*/, LPARAM lpData)	
	{	
		FONTNAMECACHE* pCache = reinterpret_cast<FONTNAMECACHE*>(lpData);
		
		CString sName(lplf->lfFaceName);
		BOOL bSeen = FALSE;
		
		if (!sName.IsEmpty() && !pCache->mapSeen.Lookup(sName, bSeen))
		{
			pCache->mapSeen[sName] = TRUE;
			pCache->aNames.Add(sName);
		}
		
		return TRUE;
	}

	void BuildFontNameCache(CDC& dc, FONTNAMECACHE& cache)
	{
		CScopedLogTimer logCache(_T("      FontCache::Build"));

		cache.aNames.RemoveAll();
		cache.mapSeen.RemoveAll();
		cache.nMaxWidth = 0;
		
		LOGFONT lf = { 0 };
		lf.lfCharSet = DEFAULT_CHARSET;
		
		::EnumFontFamiliesEx(dc.GetSafeHdc(), &lf, EnumFontFamilyProc, (LPARAM)&cache, 0);

		logCache.LogTimeElapsed(_T("      FontCache::Enumerate"));
		
		// measure once per unique font name using the same font as before
		CGdiObject* pOldFont = dc.SelectStockObject(ANSI_VAR_FONT);
		
		for (int nName = 0; nName < cache.aNames.GetSize(); nName++)
		{
			// Add a "0" for the margin.
			CSize sz = dc.GetTextExtent(cache.aNames[nName] + CString(_T("0")));
			cache.nMaxWidth = max(cache.nMaxWidth, sz.cx);
		}
		
		dc.SelectObject(pOldFont);
		
		cache.bValid = TRUE;
	}

	const FONTNAMECACHE& EnsureFontNameCache(CDC& dc)
	{
		FONTNAMECACHE& cache = GetFontNameCache();
		
		if (!cache.bValid)
			BuildFontNameCache(dc, cache);
		
		return cache;
	}
}

/////////////////////////////////////////////////////////////////////////////
// CFontComboBox

CFontNameComboBox::CFontNameComboBox()
/* ============================================================
	Function :		CFontComboBox::CFontComboBox
	Description :	ctor
	Access :		Public
					
	Return :		void
	Parameters :	none

	Usage :			

   ============================================================*/
{
	m_maxWidth = 0;
}

CFontNameComboBox::~CFontNameComboBox()
/* ============================================================
	Function :		CFontComboBox::~CFontComboBox
	Description :	dtor
	Access :		Public
					
	Return :		void
	Parameters :	none

	Usage :			

   ============================================================*/
{
}

BEGIN_MESSAGE_MAP(CFontNameComboBox, CComboBox)
	//{{AFX_MSG_MAP(CFontComboBox)
	ON_CONTROL_REFLECT(CBN_DROPDOWN, OnDropdown)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CFontComboBox message handlers

void CFontNameComboBox::OnDropdown() 
/* ============================================================
	Function :		CFontComboBox::OnDropdown
	Description :	Sets the dropped down width of the combo 
					control to the max width of the string in 
					the list
	Access :		Protected
					
	Return :		void
	Parameters :	none

	Usage :			Called from MFC

   ============================================================*/
{

   int scroll = ::GetSystemMetrics(SM_CXVSCROLL);
   SetDroppedWidth( m_maxWidth + scroll );

}

/////////////////////////////////////////////////////////////////////////////
// CFontComboBox implementation

void CFontNameComboBox::FillCombo()
/* ============================================================
	Function :		CFontComboBox::FillCombo
	Description :	Fills the combo with the font names from 
					the system
	Access :		Public
					
	Return :		void
	Parameters :	none

	Usage :			Call to fill the combo with the installed 
					fonts.

   ============================================================*/
{
	CScopedLogTimer logFill(_T("      FontCombo::FillCombo"));

	CClientDC dc( this );
	const FONTNAMECACHE& cache = EnsureFontNameCache( dc );

	logFill.LogTimeElapsed(_T("      FontCombo::EnsureCache"));
	
	SetRedraw( FALSE );
	ResetContent();

	for ( int nName = 0; nName < cache.aNames.GetSize(); nName++ )
		AddString( cache.aNames[nName] );

	SetRedraw( TRUE );
	Invalidate();

	m_maxWidth = cache.nMaxWidth;
}

void CFontNameComboBox::InvalidateFontCache()
{
	GetFontNameCache().bValid = FALSE;
}

void CFontNameComboBox::SetMaxWidth( int maxWidth )
/* ============================================================
	Function :		CFontComboBox::SetMaxWidth
	Description :	Sets the "m_maxWidth" member.
	Access :		Public
					
	Return :		void
	Parameters :	int maxWidth	-	New width of the drop
										down list

	Usage :			Call to set a max width for the strings in 
					the list, used to set the width of the drop 
					down list. Called from the font enum 
					callback.

   ============================================================*/
{

	m_maxWidth = maxWidth;

}

int CFontNameComboBox::GetMaxWidth() const
/* ============================================================
	Function :		CFontComboBox::GetMaxWidth
	Description :	Returns the "m_maxWidth" member of the 
					control.
	Access :		Public
					
	Return :		int		-	Width of the longest string, in 
								pixels.
	Parameters :	none

	Usage :			Call to get the width necessary to display 
					the longest string in the combo drop down.

   ============================================================*/
{

	return m_maxWidth;

}

BOOL CFontNameComboBox::SelectFontName( const CString& font )
/* ============================================================
	Function :		CFontComboBox::SelectFontName
	Description :	Selects "font" in the list, if it 
					exists.
	Access :		Public
					
	Return :		void
	Parameters :	const CString& font	-	Name of the font to 
											select.

	Usage :			Call to set/change the selection of the 
					combo.

   ============================================================*/
{
	int nFind = FindStringExact(-1, font);

	if (nFind != GetCurSel())
		SetCurSel(nFind);

	return (nFind != -1);
}

void CFontNameComboBox::DDX(CDataExchange* pDX, CString& font)
{
	if (pDX->m_bSaveAndValidate)
	{
		int nSel = GetCurSel();

		if (nSel == -1)
			font.Empty();
		else
			GetLBText(nSel, font);
	}
	else
		SelectString(-1, font);
}
