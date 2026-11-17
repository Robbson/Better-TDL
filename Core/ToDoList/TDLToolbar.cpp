// TDLToolbar.cpp : implementation of the data-driven toolbar builder
//

#include "stdafx.h"
#include "resource.h"
#include "TDLToolbar.h"

#include "TDCImageList.h"

#include "..\Shared\Icon.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// Main toolbar specification
//
// Order and contents of the main toolbar are defined here, not in the
// IDR_APP_TOOLBAR resource. Image keys refer to names in CTDCImageList
// ('200'..'226' are the icons in IDB_APP_TOOLBAR_STD).
//
// To add a command, add a line in the required position. If its icon is
// already in the shared image list (task icons, app toolbar strip, extra
// strip or the user's Icons folder), no bitmap changes are required.

static const TDCTOOLBARITEM MAIN_TOOLBAR_ITEMS[] =
{	
	{ ID_VIEW_PREV_SEL,				_T("213") },
	{ ID_VIEW_NEXT_SEL,				_T("214") },
	{},
	//{ ID_LOAD_NORMAL,				_T("200") },
	{ ID_SAVE_NORMAL,				_T("201") },
	{ ID_SAVEALL,					_T("202") },
	{},
	{ ID_NEWTASK,					_T("203") },
	{ ID_NEWSUBTASK,				_T("204") },
	{ ID_DELETETASK,				_T("217") },
	{},
	{ ID_EDIT_TASKTEXT,				_T("205") },
	{ ID_EDIT_SETTASKICON,			_T("206") },
	{ ID_EDIT_TASKCOLOR,				_T("56") },
	{},
	{ ID_EDIT_SETREMINDER,			_T("207") },
	{},
	{ ID_VIEW_MAXTASKLIST,			_T("7") }, // Todo: Option für horizontal/vertical window split
	{ ID_VIEW_MAXTASKLIST,			_T("210") },
	{},
	{ ID_EDIT_UNDO,					_T("208") },
	{ ID_EDIT_REDO,					_T("209") },
	//{ ID_VIEW_EXPANDTASK,			_T("211") },
	//{ ID_VIEW_COLLAPSETASK,			_T("212") },
	//{},
	{},
	{ ID_EDIT_FINDTASKS,			_T("215") },
	{},
	{ ID_SORT,						_T("216") },
	{},
	{ ID_TOOLS_TOGGLECHECKIN,		_T("218") },
	{},
	//{ ID_TOOLS_TOGGLEDARKMODE,		_T("219") },
	//{},
	//{ ID_PREFERENCES,				_T("220") },
	//{},
	//{ ID_HELP_WIKI,					_T("221") },
	//{},
	//{ ID_COMMENTS_INSERTDATE,		_T("222") },
	//{ ID_COMMENTS_INSERTTIME,		_T("223") },
	//{ ID_COMMENTS_INSERTDATETIME,	_T("224") },
	//{},
	{ ID_COMMENTS_WORDWRAP,			_T("225") },
	{ ID_COMMENTS_SHOWALLCHARS,		_T("226") },
};

static const TDCTOOLBARSPEC MAIN_TOOLBAR_SPEC =
{
	_T("Main"),
	MAIN_TOOLBAR_ITEMS,
	(int)_countof(MAIN_TOOLBAR_ITEMS),
};

/////////////////////////////////////////////////////////////////////////////

const TDCTOOLBARSPEC& GetMainToolbarSpec()
{
	return MAIN_TOOLBAR_SPEC;
}

/////////////////////////////////////////////////////////////////////////////
// CTDLToolbar

BOOL CTDLToolbar::SetImageList(HIMAGELIST hImageList, int cxImage, int cyImage)
{
	if (!hImageList)
		return FALSE;

	SetImageSize(cxImage, cyImage);

	BOOL bCleanupNormal = (GetToolBarCtrl().GetImageList() != &m_ilNormal);
	BOOL bCleanupHot = (GetToolBarCtrl().GetHotImageList() != &m_ilNormal);

	GetToolBarCtrl().SetImageList(NULL);
	GetToolBarCtrl().SetHotImageList(NULL);

	m_ilNormal.DeleteImageList();

	if (!m_ilNormal.Attach(hImageList))
	{
		::ImageList_Destroy(hImageList);
		return FALSE;
	}

	CImageList* pILPrev = GetToolBarCtrl().SetImageList(&m_ilNormal);

	if (pILPrev && bCleanupNormal)
		pILPrev->DeleteImageList();

	pILPrev = GetToolBarCtrl().SetHotImageList(&m_ilNormal);

	if (pILPrev && bCleanupHot)
		pILPrev->DeleteImageList();

	RefreshDisabledImageList();

	return TRUE;
}

/////////////////////////////////////////////////////////////////////////////
// CTDLToolbarBuilder

BOOL CTDLToolbarBuilder::Build(CTDLToolbar& toolbar, const TDCTOOLBARSPEC& spec)
{
	if (!toolbar.GetSafeHwnd() || !spec.pItems || (spec.nCount <= 0))
		return FALSE;

	COLORREF crBkgnd = toolbar.GetBackgroundColor();

	if (crBkgnd == CLR_NONE)
		crBkgnd = GetSysColor(COLOR_3DFACE);

	CTDCImageList ilSource(crBkgnd);

	if (ilSource.LoadDefaultImages(TRUE) <= 0)
		return FALSE;

	CSize sizeImage;

	if (!ilSource.GetImageSize(sizeImage) || (sizeImage.cx <= 0) || (sizeImage.cy <= 0))
		return FALSE;

	CEnImageList ilBar;

	if (!ilBar.Create(sizeImage.cx, sizeImage.cy, ILC_COLOR32 | ILC_MASK, 0, 1))
		return FALSE;

	CArray<int, int&> aImages;
	aImages.SetSize(spec.nCount);

	for (int nItem = 0; nItem < spec.nCount; nItem++)
	{
		const TDCTOOLBARITEM& item = spec.pItems[nItem];
		aImages[nItem] = -1;

		if (!item.nCmdID)
			continue;

		ASSERT(item.szImageKey && *item.szImageKey);

		int nSource = ilSource.GetImageIndex(item.szImageKey);
		ASSERT(nSource != -1);

		if (nSource == -1)
			continue;

		CIcon icon(ilSource.ExtractIcon(nSource));

		if (!icon.IsValid())
			return FALSE;

		aImages[nItem] = ilBar.Add(icon, crBkgnd);

		if (aImages[nItem] == -1)
			return FALSE;
	}

	if (!toolbar.SetImageList(ilBar.Detach(), 16, 16))
		return FALSE;

	for (int nItem = 0; nItem < spec.nCount; nItem++)
	{
		const TDCTOOLBARITEM& item = spec.pItems[nItem];
		TBBUTTON tbb = { -1, (int)item.nCmdID, 0, TBSTYLE_SEP, 0, 0, (UINT)-1 };

		if (item.nCmdID)
		{
			tbb.iBitmap = aImages[nItem];
			tbb.fsStyle = TBSTYLE_BUTTON;
		}

		if (!toolbar.GetToolBarCtrl().InsertButton(nItem, &tbb))
			return FALSE;
	}

	return TRUE;
}
