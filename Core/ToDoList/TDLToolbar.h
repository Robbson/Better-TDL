#if !defined(AFX_TDLTOOLBAR_H__2F2B2B4B_1E6B_4B9A_9C5E_6D0B6E6B7C11__INCLUDED_)
#define AFX_TDLTOOLBAR_H__2F2B2B4B_1E6B_4B9A_9C5E_6D0B6E6B7C11__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "..\Shared\EnToolBar.h"

/////////////////////////////////////////////////////////////////////////////

struct TDCTOOLBARITEM
{
	TDCTOOLBARITEM(UINT nID = 0, LPCTSTR szImage = NULL)
		:
		nCmdID(nID),
		szImageKey(szImage)
	{
	}

	UINT nCmdID;		// 0 == separator
	LPCTSTR szImageKey;	// image name in CTDCImageList
};

/////////////////////////////////////////////////////////////////////////////

struct TDCTOOLBARSPEC
{
	LPCTSTR szName;
	const TDCTOOLBARITEM* pItems;
	int nCount;
};

/////////////////////////////////////////////////////////////////////////////

class CTDLToolbar : public CEnToolBar
{
public:
	COLORREF GetBackgroundColor() const { return m_crFrom; }

	// Takes ownership of hImageList
	BOOL SetImageList(HIMAGELIST hImageList, int cxImage, int cyImage);
};

/////////////////////////////////////////////////////////////////////////////

const TDCTOOLBARSPEC& GetMainToolbarSpec();

class CTDLToolbarBuilder
{
public:
	static BOOL Build(CTDLToolbar& toolbar, const TDCTOOLBARSPEC& spec);
};

/////////////////////////////////////////////////////////////////////////////

#endif // !defined(AFX_TDLTOOLBAR_H__2F2B2B4B_1E6B_4B9A_9C5E_6D0B6E6B7C11__INCLUDED_)
