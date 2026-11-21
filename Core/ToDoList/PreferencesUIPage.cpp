// PreferencesUIPage.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "PreferencesUIPage.h"
#include "tdcenum.h"
#include "TDLSimpleTextContentCtrl.h"

#include "..\shared\misc.h"
#include "..\shared\enstring.h"
#include "..\shared\themed.h"
#include "..\shared\filemisc.h"
#include "..\shared\darkmode.h"

#include "..\Interfaces\Preferences.h"

#include "..\3rdParty\OSVersion.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////

const LPCTSTR COMMENT_BULLET_PREFIXES_KEY = _T("Preferences\\CommentBulletPrefixes");

static void SplitBulletPrefixes(LPCTSTR szPrefixes, CStringArray& aPrefixes)
{
	aPrefixes.RemoveAll();

	CString sPrefixes(szPrefixes);
	sPrefixes.Replace(_T("\r\n"), _T("\n"));
	sPrefixes.Replace(_T('\r'), _T('\n'));

	int nStart = 0;

	while (nStart <= sPrefixes.GetLength())
	{
		int nEnd = sPrefixes.Find(_T('\n'), nStart);

		if (nEnd < 0)
			nEnd = sPrefixes.GetLength();

		CString sPrefix = sPrefixes.Mid(nStart, nEnd - nStart);
		CString sTrimmed(sPrefix);

		if (!Misc::Trim(sTrimmed).IsEmpty())
			aPrefixes.Add(sPrefix);

		if (nEnd == sPrefixes.GetLength())
			break;

		nStart = nEnd + 1;
	}
}

/////////////////////////////////////////////////////////////////////////////
// CPreferencesUIPage property page

CPreferencesUIPage::CPreferencesUIPage(const CUIExtensionMgr* pMgrUIExt) 
	: 
	CPreferencesPageBase(IDD_PREFUI_PAGE),
	m_pMgrUIExt(pMgrUIExt),
	m_lbTaskViews(pMgrUIExt)
{
	//{{AFX_DATA_INIT(CPreferencesUIPage)
	//}}AFX_DATA_INIT
}

CPreferencesUIPage::~CPreferencesUIPage()
{
}

void CPreferencesUIPage::DoDataExchange(CDataExchange* pDX)
{
	CPreferencesPageBase::DoDataExchange(pDX);

	//{{AFX_DATA_MAP(CPreferencesUIPage)
	DDX_Check(pDX, IDC_SHOWCOMMENTSALWAYS, m_bShowCommentsAlways);
	DDX_Check(pDX, IDC_SHAREDCOMMENTSHEIGHT, m_bShareCommentsSize);
	DDX_Check(pDX, IDC_AUTOHIDETABBAR, m_bAutoHideTabbar);
	DDX_Check(pDX, IDC_STACKTABBARITEMS, m_bStackTabbarItems);
	DDX_Check(pDX, IDC_FOCUSTREEONENTER, m_bFocusTreeOnEnter);
	DDX_Check(pDX, IDC_KEEPTABSORDERED, m_bKeepTabsOrdered);
	DDX_Check(pDX, IDC_SHOWTASKLISTCLOSEBTN, m_bShowTabCloseButtons);
	DDX_Check(pDX, IDC_SHOWEDITMENUASCOLUMNS, m_bShowEditMenuAsColumns);
	DDX_Check(pDX, IDC_MULTISELFILTER, m_bMultiSelFilters);
	DDX_Check(pDX, IDC_RESTORETASKLISTFILTERS, m_bRestoreTasklistFilters);
	DDX_Check(pDX, IDC_AUTOREFILTER, m_bAutoRefilter);
	DDX_Check(pDX, IDC_USEUITHEME, m_bUseUITheme);
	DDX_Check(pDX, IDC_SHOWDEFAULTTASKICONS, m_bShowDefaultTaskIcons);
	DDX_Check(pDX, IDC_SHOWDEFAULTFILTERS, m_bShowDefaultFilters);
	DDX_Check(pDX, IDC_AUTORESORT, m_bAutoResort);
	DDX_Check(pDX, IDC_ENABLECOLUMNHEADERSORTING, m_bEnableColumnHeaderSorting);
	//}}AFX_DATA_MAP
	DDX_Check(pDX, IDC_STACKFIELDSANDCOMMENTS, m_bStackEditFieldsAndComments);
	DDX_Check(pDX, IDC_STACKCOMMENTSABOVEFIELDS, m_bStackCommentsAboveEditFields);
	DDX_Control(pDX, IDC_UITHEMEFILE, m_cbThemes);
	DDX_Radio(pDX, IDC_MATCHTITLES, (int&)m_nTitleFilterOption);
	DDX_CBIndex(pDX, IDC_NEWTASKPOSITION, (int&)m_nNewTaskPos);
	DDX_CBIndex(pDX, IDC_NEWSUBTASKPOSITION, (int&)m_nNewSubtaskPos);
	DDX_CBIndex(pDX, IDC_COMMENTPOS, (int&)m_nCommentsPos);
	DDX_CBIndex(pDX, IDC_CTRLSPOS, (int&)m_nCtrlsPos);
	DDX_Control(pDX, IDC_TASKVIEWVISIBILITY, m_lbTaskViews);
	DDX_Check(pDX, IDC_SORTDONETASKSATBOTTOM, m_bSortDoneTasksAtBottom);
	DDX_Check(pDX, IDC_INCLUDEWEBLINKINCOMMENTSPASTE, m_bIncludeWebLinksInCommentsPaste);
	DDX_Check(pDX, IDC_INSERTSPACESFORCOMMENTSTABS, m_bInsertSpacesForCommentsIndent);
	DDX_Text(pDX, IDC_COMMENTBULLETPREFIXES, m_sCommentBulletPrefixes);
	DDX_Check(pDX, IDC_USELEGACYATTRIBUTEEDITOR, m_bUseLegacyAttributeEditor);
	DDX_Check(pDX, IDC_USELEGACYROWHEIGHT, m_bUseLegacyRowHeight);
	DDX_Check(pDX, IDC_USELEGACYHORIZONTALPADDING, m_bUseLegacyHorizontalPadding);

	m_cbThemes.DDX(pDX, m_sUIThemeFile);
}

BEGIN_MESSAGE_MAP(CPreferencesUIPage, CPreferencesPageBase)
	//{{AFX_MSG_MAP(CPreferencesUIPage)
	ON_BN_CLICKED(IDC_USEUITHEME, OnUseuitheme)
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDC_STACKFIELDSANDCOMMENTS, OnStackEditFieldsAndComments)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CPreferencesUIPage message handlers

BOOL CPreferencesUIPage::OnInitDialog() 
{
	CPreferencesPageBase::OnInitDialog();
	
	return TRUE;  // return TRUE unless you set the focus to a control
	              // EXCEPTION: OCX Property Pages should return FALSE
}

void CPreferencesUIPage::InitTooltips()
{
	AddTooltip(IDC_CTRLSPOS, IDS_PREFTIP_UI_CTRLSPOS);
	AddTooltip(IDC_COMMENTPOS, IDS_PREFTIP_UI_COMMENTPOS);
	AddTooltip(IDC_STACKFIELDSANDCOMMENTS, IDS_PREFTIP_UI_STACKFIELDS);
	AddTooltip(IDC_STACKCOMMENTSABOVEFIELDS, IDS_PREFTIP_UI_STACKCOMMENTS);
	AddTooltip(IDC_SHOWEDITMENUASCOLUMNS, IDS_PREFTIP_UI_SHOWEDITMENU);
	AddTooltip(IDC_FOCUSTREEONENTER, IDS_PREFTIP_UI_FOCUSENTER);
	AddTooltip(IDC_USEUITHEME, IDS_PREFTIP_UI_THEME);
	AddTooltip(IDC_UITHEMEFILE, IDS_PREFTIP_UI_THEME);
	AddTooltip(IDC_SHOWDEFAULTTASKICONS, IDS_PREFTIP_UI_DEFAULTICONS);
	AddTooltip(IDC_SHOWTASKLISTCLOSEBTNONTAB, IDS_PREFTIP_UI_CLOSEBTN);
	AddTooltip(IDC_SHOWDEFAULTFILTERS, IDS_PREFTIP_UI_SHOWDEFAULTFILTERS);
	AddTooltip(IDC_MULTISELFILTER, IDS_PREFTIP_UI_MULTISEL);
	AddTooltip(IDC_RESTORETASKLISTFILTERS, IDS_PREFTIP_UI_RESTOREFILTERS);
	AddTooltip(IDC_AUTOREFILTER, IDS_PREFTIP_UI_REFILTER);
	AddTooltip(IDC_MATCHTITLES, IDS_PREFTIP_UI_MATCHTITLES);
	AddTooltip(IDC_MATCHTITLESCOMMENTS, IDS_PREFTIP_UI_MATCHTITLESCOMMENTS);
	AddTooltip(IDC_MATCHANYTEXT, IDS_PREFTIP_UI_MATCHANYTEXT);
	AddTooltip(IDC_SORTDONETASKSATBOTTOM, IDS_PREFTIP_UI_SORTDONE);
	AddTooltip(IDC_AUTORESORT, IDS_PREFTIP_UI_RESORT);
	AddTooltip(IDC_ENABLECOLUMNHEADERSORTING, IDS_PREFTIP_UI_COLSORT);
	AddTooltip(IDC_NEWTASKPOSITION, IDS_PREFTIP_UI_NEWTASKPOS);
	AddTooltip(IDC_NEWSUBTASKPOSITION, IDS_PREFTIP_UI_NEWSUBTASKPOS);
	AddTooltip(IDC_SHOWCOMMENTSALWAYS, IDS_PREFTIP_UI_SHOWCOMMENTS);
	AddTooltip(IDC_SHAREDCOMMENTSHEIGHT, IDS_PREFTIP_UI_SHARECOMMENTS);
	AddTooltip(IDC_INCLUDEWEBLINKINCOMMENTSPASTE, IDS_PREFTIP_UI_WEBLINKPASTE);
	AddTooltip(IDC_COMMENTBULLETPREFIXES, IDS_PREFTIP_UI_BULLETPREFIXES);
	AddTooltip(IDC_AUTOHIDETABBAR, IDS_PREFTIP_UI_TABBARHIDE);
	AddTooltip(IDC_STACKTABBARITEMS, IDS_PREFTIP_UI_TABBARSTACK);
	AddTooltip(IDC_KEEPTABSORDERED, IDS_PREFTIP_UI_TABORDER);
	AddTooltip(IDC_TASKVIEWVISIBILITY, IDS_PREFTIP_UI_DEFAULTVIEWS);
	AddTooltip(IDC_USELEGACYROWHEIGHT, IDS_PREFTIP_UI_LEGACYROWHEIGHT);
	AddTooltip(IDC_USELEGACYHORIZONTALPADDING, IDS_PREFTIP_UI_LEGACYHORIZONTALPADDING);
}

void CPreferencesUIPage::AddSearchText(CStringArray& aText) const
{
	aText.Add(m_sUIThemeFile);
	aText.Add(m_sCommentBulletPrefixes);
}

void CPreferencesUIPage::OnFirstShow()
{
	CPreferencesPageBase::OnFirstShow();

	AddGroupLine(IDC_GENERALGROUP);
	AddGroupLine(IDC_LEGACYGROUP);
	AddGroupLine(IDC_TOOLBARGROUP);
	AddGroupLine(IDC_SORTGROUP);
	AddGroupLine(IDC_TABBARGROUP);
	AddGroupLine(IDC_COMMENTSGROUP);
	AddGroupLine(IDC_FILTERGROUP);
	AddGroupLine(IDC_TASKVIEWSGROUP);

	GetDlgItem(IDC_STACKCOMMENTSABOVEFIELDS)->EnableWindow(m_bStackEditFieldsAndComments);

	// theming only available if XP themes are active
	if (CThemed::IsAppThemed() && !CDarkMode::IsEnabled())
	{
		m_cbThemes.SetThemePath(m_sUIThemeFile);
		GetDlgItem(IDC_UITHEMEFILE)->EnableWindow(m_bUseUITheme);
	}
	else
	{
		GetDlgItem(IDC_USEUITHEME)->EnableWindow(FALSE);
		GetDlgItem(IDC_UITHEMEFILE)->EnableWindow(FALSE);
	}
	
	m_lbTaskViews.ModifyStyleEx(0, WS_EX_CLIENTEDGE);
}

void CPreferencesUIPage::OnStackEditFieldsAndComments() 
{
	UpdateData();

	GetDlgItem(IDC_STACKCOMMENTSABOVEFIELDS)->EnableWindow(m_bStackEditFieldsAndComments);

	CPreferencesPageBase::OnControlChange();
}

void CPreferencesUIPage::LoadPreferences(const IPreferences* pPrefs, LPCTSTR szKey)
{
	m_bShowEditMenuAsColumns = pPrefs->GetProfileInt(szKey, _T("ShowEditMenuAsColumns"), FALSE);
	m_bShowCommentsAlways = pPrefs->GetProfileInt(szKey, _T("ShowCommentsAlways"), FALSE);
	m_bSpecifyToolbarImage = pPrefs->GetProfileInt(szKey, _T("SpecifyToolbarImage"), FALSE);
	m_bShareCommentsSize = pPrefs->GetProfileInt(szKey, _T("SharedCommentsHeight"), TRUE);
	m_bAutoHideTabbar = pPrefs->GetProfileInt(szKey, _T("AutoHideTabbar"), TRUE);
	m_bStackTabbarItems = pPrefs->GetProfileInt(szKey, _T("StackTabbarItems"), FALSE);
	m_bFocusTreeOnEnter = pPrefs->GetProfileInt(szKey, _T("FocusTreeOnEnter"), FALSE);
	m_nNewTaskPos = pPrefs->GetProfileEnum(szKey, _T("NewTaskPos"), PUIP_BELOW);
	m_nNewSubtaskPos = pPrefs->GetProfileEnum(szKey, _T("NewSubtaskPos"), PUIP_BOTTOM);
	m_bKeepTabsOrdered = pPrefs->GetProfileInt(szKey, _T("KeepTabsOrdered"), FALSE);
	m_bShowTabCloseButtons = pPrefs->GetProfileInt(szKey, _T("ShowTasklistTabCloseButton"), TRUE);
	m_bSortDoneTasksAtBottom = pPrefs->GetProfileInt(szKey, _T("SortDoneTasksAtBottom"), TRUE);
	m_bRTLComments = pPrefs->GetProfileInt(szKey, _T("RTLComments"), FALSE);
	m_nCommentsPos = pPrefs->GetProfileEnum(szKey, _T("VertComments"), PUIP_LOCATERIGHT);
	m_nCtrlsPos = pPrefs->GetProfileEnum(szKey, _T("VertControls"), PUIP_LOCATERIGHT);
	m_bMultiSelFilters = pPrefs->GetProfileInt(szKey, _T("MultiSelFilters"), TRUE);
	m_bRestoreTasklistFilters = pPrefs->GetProfileInt(szKey, _T("RestoreTasklistFilters"), TRUE);
	m_bAutoRefilter = pPrefs->GetProfileInt(szKey, _T("AutoRefilter"), TRUE);
	m_bAutoResort = pPrefs->GetProfileInt(szKey, _T("AutoResort"), TRUE);
	m_bUseUITheme = CThemed::IsAppThemed() && pPrefs->GetProfileInt(szKey, _T("UseUITheme"), TRUE);
	m_sUIThemeFile = pPrefs->GetProfileString(szKey, _T("UIThemeFile"), _T(".\\ThemeBlue.xml"));
	m_nTitleFilterOption = pPrefs->GetProfileEnum(szKey, _T("TitleFilterOption"), PUIP_MATCHONTITLECOMMENTS);
	m_bShowDefaultTaskIcons = pPrefs->GetProfileInt(szKey, _T("ShowDefaultTaskIcons"), TRUE);
	m_bShowDefaultFilters = pPrefs->GetProfileInt(szKey, _T("ShowDefaultFilters"), TRUE);
	m_bEnableColumnHeaderSorting = pPrefs->GetProfileInt(szKey, _T("EnableColumnHeaderSorting"), TRUE);
	m_bStackEditFieldsAndComments = pPrefs->GetProfileInt(szKey, _T("StackEditFieldsAndComments"), TRUE);
	m_bStackCommentsAboveEditFields = pPrefs->GetProfileInt(szKey, _T("StackCommentsAboveEditFields"), TRUE);
	m_bIncludeWebLinksInCommentsPaste = pPrefs->GetProfileInt(szKey, _T("IncludeWebLinksInCommentsPaste"), TRUE);
	m_bInsertSpacesForCommentsIndent = pPrefs->GetProfileInt(szKey, _T("InsertSpacesForCommentsIndent"), TRUE);
	m_bUseLegacyAttributeEditor = pPrefs->GetProfileInt(szKey, _T("AttributeEditorMode"), 0) != 0;
	m_bUseLegacyRowHeight = pPrefs->GetProfileInt(szKey, _T("LegacyRowHeight"), FALSE);
	m_bUseLegacyHorizontalPadding = pPrefs->GetProfileInt(szKey, _T("LegacyHorizontalPadding"), FALSE);

	CStringArray aBulletPrefixes;

	if (pPrefs->GetProfileInt(COMMENT_BULLET_PREFIXES_KEY, _T("ItemCount"), -1) >= 0)
		CPreferences::GetProfileArray(pPrefs, COMMENT_BULLET_PREFIXES_KEY, aBulletPrefixes, TRUE);

	if (aBulletPrefixes.GetSize() == 0)
		CTDLSimpleTextContentCtrl::GetDefaultBulletPrefixes(aBulletPrefixes);

	m_sCommentBulletPrefixes = Misc::FormatArray(aBulletPrefixes, _T("\r\n"));

	// First time handling of new 'Attribute Editor'
	// If the attributes were previously on the bottom move them to the side
	if ((m_nCtrlsPos == PUIP_LOCATEBOTTOM) && !pPrefs->GetProfileInt(szKey, _T("AttributeEditor"), FALSE))
	{
		if (m_nCommentsPos != PUIP_LOCATEBOTTOM)
			m_nCtrlsPos = m_nCommentsPos;
		else
			m_nCtrlsPos = PUIP_LOCATERIGHT;
	}
	
	// task view visibility
	CStringArray aViews;
	int nView = pPrefs->GetProfileInt(_T("Preferences\\ViewVisibility"), _T("HiddenCount"), -1);

	if (nView > 0)
	{
		while (nView--)
		{
			CString sKey = Misc::MakeKey(_T("Hidden%d"), nView);
			CString sView = pPrefs->GetProfileString(_T("Preferences\\ViewVisibility"), sKey);
			ASSERT(!sView.IsEmpty());
			
			aViews.Add(sView);
		}

		m_lbTaskViews.SetHiddenViews(aViews);
	}
}

void CPreferencesUIPage::SavePreferences(IPreferences* pPrefs, LPCTSTR szKey) const
{
	pPrefs->WriteProfileInt(szKey, _T("ShowEditMenuAsColumns"), m_bShowEditMenuAsColumns);
	pPrefs->WriteProfileInt(szKey, _T("ShowCommentsAlways"), m_bShowCommentsAlways);
	pPrefs->WriteProfileInt(szKey, _T("SpecifyToolbarImage"), m_bSpecifyToolbarImage);
	pPrefs->WriteProfileInt(szKey, _T("SharedCommentsHeight"), m_bShareCommentsSize);
	pPrefs->WriteProfileInt(szKey, _T("AutoHideTabbar"), m_bAutoHideTabbar);
	pPrefs->WriteProfileInt(szKey, _T("StackTabbarItems"), m_bStackTabbarItems);
	pPrefs->WriteProfileInt(szKey, _T("FocusTreeOnEnter"), m_bFocusTreeOnEnter);
	pPrefs->WriteProfileInt(szKey, _T("NewTaskPos"), m_nNewTaskPos);
	pPrefs->WriteProfileInt(szKey, _T("NewSubtaskPos"), m_nNewSubtaskPos);
	pPrefs->WriteProfileInt(szKey, _T("KeepTabsOrdered"), m_bKeepTabsOrdered);
	pPrefs->WriteProfileInt(szKey, _T("ShowTasklistTabCloseButton"), m_bShowTabCloseButtons);
	pPrefs->WriteProfileInt(szKey, _T("SortDoneTasksAtBottom"), m_bSortDoneTasksAtBottom);
	pPrefs->WriteProfileInt(szKey, _T("RTLComments"), m_bRTLComments);
	pPrefs->WriteProfileInt(szKey, _T("VertComments"), m_nCommentsPos);
	pPrefs->WriteProfileInt(szKey, _T("VertControls"), m_nCtrlsPos);
	pPrefs->WriteProfileInt(szKey, _T("AttributeEditorMode"), (m_bUseLegacyAttributeEditor ? 1 : 0));
	pPrefs->WriteProfileInt(szKey, _T("LegacyRowHeight"), m_bUseLegacyRowHeight);
	pPrefs->WriteProfileInt(szKey, _T("LegacyHorizontalPadding"), m_bUseLegacyHorizontalPadding);
	pPrefs->WriteProfileInt(szKey, _T("MultiSelFilters"), m_bMultiSelFilters);
	pPrefs->WriteProfileInt(szKey, _T("RestoreTasklistFilters"), m_bRestoreTasklistFilters);
	pPrefs->WriteProfileInt(szKey, _T("AutoRefilter"), m_bAutoRefilter);
	pPrefs->WriteProfileInt(szKey, _T("AutoResort"), m_bAutoResort);
	pPrefs->WriteProfileInt(szKey, _T("UseUITheme"), m_bUseUITheme);
	pPrefs->WriteProfileInt(szKey, _T("TitleFilterOption"), m_nTitleFilterOption);
	pPrefs->WriteProfileInt(szKey, _T("ShowDefaultTaskIcons"), m_bShowDefaultTaskIcons);
	pPrefs->WriteProfileInt(szKey, _T("ShowDefaultFilters"), m_bShowDefaultFilters);
	pPrefs->WriteProfileInt(szKey, _T("EnableColumnHeaderSorting"), m_bEnableColumnHeaderSorting);
	pPrefs->WriteProfileInt(szKey, _T("StackEditFieldsAndComments"), m_bStackEditFieldsAndComments);
	pPrefs->WriteProfileInt(szKey, _T("StackCommentsAboveEditFields"), m_bStackCommentsAboveEditFields);
	pPrefs->WriteProfileInt(szKey, _T("IncludeWebLinksInCommentsPaste"), m_bIncludeWebLinksInCommentsPaste);
	pPrefs->WriteProfileInt(szKey, _T("InsertSpacesForCommentsIndent"), m_bInsertSpacesForCommentsIndent);
	pPrefs->WriteProfileInt(szKey, _T("AttributeEditor"), TRUE);

	CStringArray aBulletPrefixes;
	SplitBulletPrefixes(m_sCommentBulletPrefixes, aBulletPrefixes);

	CPreferences::WriteProfileArray(pPrefs, COMMENT_BULLET_PREFIXES_KEY, aBulletPrefixes);

	pPrefs->WriteProfileString(szKey, _T("UIThemeFile"), m_sUIThemeFile);

	// task views
	pPrefs->DeleteProfileSection(_T("Preferences\\ViewVisibility"));

	CStringArray aViews;
	int nView = m_lbTaskViews.GetHiddenViews(aViews);

	pPrefs->WriteProfileInt(_T("Preferences\\ViewVisibility"), _T("HiddenCount"), nView);

	while (nView--)
	{
		CString sKey = Misc::MakeKey(_T("Hidden%d"), nView);
		pPrefs->WriteProfileString(_T("Preferences\\ViewVisibility"), sKey, aViews[nView]);
	}
}

void CPreferencesUIPage::OnUseuitheme() 
{
	UpdateData();
	GetDlgItem(IDC_UITHEMEFILE)->EnableWindow(m_bUseUITheme);

	CPreferencesPageBase::OnControlChange();
}

CString CPreferencesUIPage::GetUITheme() const 
{ 
	if (m_bUseUITheme && !CDarkMode::IsEnabled())
		return FileMisc::GetFullPath(m_sUIThemeFile, FileMisc::GetAppResourceFolder() + "\\Themes");
	
	// else
	return _T("");
}

int CPreferencesUIPage::GetDefaultTaskViews(CStringArray& aTypeIDs) const
{
	return m_lbTaskViews.GetVisibleViews(aTypeIDs);
}

BOOL CPreferencesUIPage::SetAutoHideTabbar(BOOL bAutoHide)
{
	if ((m_bAutoHideTabbar && bAutoHide) || (!m_bAutoHideTabbar && !bAutoHide))
		return FALSE;

	m_bAutoHideTabbar = bAutoHide;
	return TRUE;
}

BOOL CPreferencesUIPage::SetCommentsPos(PUIP_LOCATION nPos)
{
	if (m_nCommentsPos == nPos)
		return FALSE;

	m_nCommentsPos = nPos;
	return TRUE;
}

