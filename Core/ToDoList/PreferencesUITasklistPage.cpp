// PreferencesUITasklistPage.cpp : implementation file
//

#include "stdafx.h"
#include "resource.h"
#include "PreferencesUITasklistPage.h"

#include "..\shared\misc.h"

#include "..\3rdparty\colordef.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CPreferencesUITasklistPage property page

CPreferencesUITasklistPage::CPreferencesUITasklistPage() 
	: 
	CPreferencesPageBase(IDD_PREFUITASKLIST_PAGE),
	m_cbPriorityRiskLevels(-1)
{
}

CPreferencesUITasklistPage::~CPreferencesUITasklistPage()
{
}

void CPreferencesUITasklistPage::DoDataExchange(CDataExchange* pDX)
{ 
	CPreferencesPageBase::DoDataExchange(pDX);

	DDX_Text(pDX, IDC_COLWIDTHS, m_nMaxColumnWidth);
	DDX_Text(pDX, IDC_DATETIMETRAIL, m_sDateTimeTrailText);
	DDX_Text(pDX, IDC_INFOTIPCOMMENTSMAX, m_nMaxInfoTipCommentsLength);

	DDX_Check(pDX, IDC_USEISODATEFORMAT, m_bUseISOForDates);
	DDX_Check(pDX, IDC_SHOWWEEKDAYINDATES, m_bShowWeekdayInDates);
	DDX_Check(pDX, IDC_SHOWPARENTSASFOLDERS, m_bShowParentsAsFolders);
	DDX_Check(pDX, IDC_DISPLAYFIRSTCOMMENTLINE, m_bDisplayFirstCommentLine);
	DDX_Check(pDX, IDC_LIMITINFOTIPCOMMENTS, m_bLimitInfoTipCommentsLength);
	DDX_Check(pDX, IDC_AUTOFOCUSTASKLIST, m_bAutoFocusTasklist);
	DDX_Check(pDX, IDC_RIGHTSIDECOLUMNS, m_bShowColumnsOnRight);
	DDX_Check(pDX, IDC_LIMITCOLWIDTHS, m_bLimitColumnWidths);
	DDX_Check(pDX, IDC_HIDELISTPARENTS, m_bHideListParents);
	DDX_Check(pDX, IDC_HIDEZEROPERCENT, m_bHideZeroPercentDone);
	DDX_Check(pDX, IDC_APPENDUSERTODATETIMEPASTE, m_bAppendUserToDateTimePaste);
	DDX_Check(pDX, IDC_APPENDTEXTTODATETIMEPASTE, m_bAppendTextToDateTimePaste);
	DDX_Check(pDX, IDC_SHOWREMINDERSASDATES, m_bShowRemindersAsDateAndTime);
	DDX_Check(pDX, IDC_DISPLAYPATHINHEADER, m_bShowPathInHeader);
	DDX_Check(pDX, IDC_STRIKETHROUGHDONE, m_bStrikethroughDone);
	DDX_Check(pDX, IDC_SHOWIMAGETIPS, m_bShowImageTips);
	DDX_Check(pDX, IDC_SHOWINFOTIPS, m_bShowInfoTips);
	DDX_Check(pDX, IDC_SHOWCOMMENTS, m_bShowComments);
	DDX_Check(pDX, IDC_HIDEUNDEFINEDTIMEST, m_bHideZeroTimeCost);
	DDX_Check(pDX, IDC_HIDESTARTDUEFORDONETASKS, m_bHideStartDueForDoneTasks);
	DDX_Check(pDX, IDC_ROUNDTIMEFRACTIONS, m_bRoundTimeFractions);
	DDX_Check(pDX, IDC_USEHMSTIMEFORMAT, m_bUseHMSTimeFormat);
	DDX_Check(pDX, IDC_SHOWPERCENTPROGRESSBAR, m_bShowPercentAsProgressbar);
	DDX_Check(pDX, IDC_HIDEPERCENTFORDONETASKS, m_bHidePercentForDoneTasks);
	DDX_Check(pDX, IDC_ALLOWTREEITEMCHECKBOX, m_bAllowCheckboxAgainstTreeItem);
	DDX_Check(pDX, IDC_HIDEPANESPLITBAR, m_bHidePaneSplitBar);
	DDX_Check(pDX, IDC_FILELINKTHUMBNAILS, m_bShowFileLinkThumbnails);
	DDX_Check(pDX, IDC_HIDEPRIORITYNUMBER, m_bHidePriorityNumber);
	DDX_Check(pDX, IDC_SHOWMIXEDDONECHECKBOX, m_bShowMixedCompletionState);
	DDX_Check(pDX, IDC_SETNUMPRIORITYRISKLEVELS, m_bSetNumPriorityRiskLevels);

	DDX_Control(pDX, IDC_PERCENTINCREMENT, m_cbPercentIncrement);
	DDX_Control(pDX, IDC_NUMPRORITYRISKLEVELS, m_cbPriorityRiskLevels);

	CDialogHelper::DDX_CBValue(pDX, m_cbPercentIncrement, m_nPercentIncrement, 5);
	CDialogHelper::DDX_CBValue(pDX, m_cbPriorityRiskLevels, m_nNumPriorityRiskLevels, 5);
}

BEGIN_MESSAGE_MAP(CPreferencesUITasklistPage, CPreferencesPageBase)
	ON_BN_CLICKED(IDC_SHOWCOMMENTS, OnShowcomments)
	ON_BN_CLICKED(IDC_SHOWINFOTIPS, OnShowinfotips)
	ON_BN_CLICKED(IDC_LIMITINFOTIPCOMMENTS, OnLimitinfotipcomments)
	ON_BN_CLICKED(IDC_LIMITCOLWIDTHS, OnLimitcolwidths)
	ON_BN_CLICKED(IDC_SHOWPARENTSASFOLDERS, OnShowparentsasfolders)
	ON_BN_CLICKED(IDC_APPENDTEXTTODATETIMEPASTE, OnAppendTextToDateTimePaste)
	ON_BN_CLICKED(IDC_SETNUMPRIORITYRISKLEVELS, OnSetNumPriorityRiskLevels)
	ON_CBN_SELCHANGE(IDC_SETNUMPRIORITYRISKLEVELS, OnSelChangeNumPriorityRiskLevels)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////

BOOL CPreferencesUITasklistPage::OnInitDialog() 
{
	CPreferencesPageBase::OnInitDialog();

	return TRUE; 
}

void CPreferencesUITasklistPage::InitTooltips()
{
	AddTooltip(IDC_HIDESTARTDUEFORDONETASKS, IDS_PREFTIP_UT_HIDESTARTDUE);
	AddTooltip(IDC_USEISODATEFORMAT, IDS_PREFTIP_UT_ISO);
	AddTooltip(IDC_SHOWWEEKDAYINDATES, IDS_PREFTIP_UT_WEEKDAY);
	AddTooltip(IDC_ROUNDTIMEFRACTIONS, IDS_PREFTIP_UT_ROUNDTIME);
	AddTooltip(IDC_USEHMSTIMEFORMAT, IDS_PREFTIP_UT_HMS);
	AddTooltip(IDC_APPENDUSERTODATETIMEPASTE, IDS_PREFTIP_UT_APPENDUSER);
	AddTooltip(IDC_APPENDTEXTTODATETIMEPASTE, IDS_PREFTIP_UT_APPENDTEXT);
	AddTooltip(IDC_DATETIMETRAILTEXT, IDS_PREFTIP_UT_APPENDTEXT);
	AddTooltip(IDC_SHOWMIXEDDONECHECKBOX, IDS_PREFTIP_UT_MIXEDCHECKBOX);
	AddTooltip(IDC_ALLOWTREEITEMCHECKBOX, IDS_PREFTIP_UT_CHECKBOX);
	AddTooltip(IDC_STRIKETHROUGHDONE, IDS_PREFTIP_UT_STRIKE);
	AddTooltip(IDC_HIDEPERCENTFORDONETASKS, IDS_PREFTIP_UT_HIDEPERCENT);
	AddTooltip(IDC_SHOWPERCENTPROGRESSBAR, IDS_PREFTIP_UT_PROGRESSBAR);
	AddTooltip(IDC_HIDEZEROPERCENT, IDS_PREFTIP_UT_HIDEZEROPERCENT);
	AddTooltip(IDC_PERCENTINCREMENT, IDS_PREFTIP_UT_PERCENTINC);
	AddTooltip(IDC_HIDEUNDEFINEDTIMEST, IDS_PREFTIP_UT_HIDEZEROTIME);
	AddTooltip(IDC_HIDEPRIORITYNUMBER, IDS_PREFTIP_UT_HIDEPRIORITY);
	AddTooltip(IDC_SETNUMPRIORITYRISKLEVELS, IDS_PREFTIP_UT_LIMITLEVELS);
	AddTooltip(IDC_NUMPRORITYRISKLEVELS, IDS_PREFTIP_UT_LIMITLEVELS);
	AddTooltip(IDC_SHOWREMINDERSASDATES, IDS_PREFTIP_UT_REMINDERSDATE);
	AddTooltip(IDC_HIDEPANESPLITBAR, IDS_PREFTIP_UT_HIDESPLITTER);
	AddTooltip(IDC_SHOWINFOTIPS, IDS_PREFTIP_UT_INFOTIPS);
	AddTooltip(IDC_LIMITINFOTIPCOMMENTS, IDS_PREFTIP_UT_INFOTIPLIMIT);
	AddTooltip(IDC_INFOTIPCOMMENTSMAX, IDS_PREFTIP_UT_INFOTIPLIMIT);
	AddTooltip(IDC_SHOWCOMMENTS, IDS_PREFTIP_UT_SHOWCOMMENTS);
	AddTooltip(IDC_DISPLAYFIRSTCOMMENTLINE, IDS_PREFTIP_UT_FIRSTLINE);
	AddTooltip(IDC_SHOWPARENTSASFOLDERS, IDS_PREFTIP_UT_FOLDERS);
	AddTooltip(IDC_RIGHTSIDECOLUMNS, IDS_PREFTIP_UT_RIGHTSIDECOLS);
	AddTooltip(IDC_AUTOFOCUSTASKLIST, IDS_PREFTIP_UT_AUTOFOCUS);
	AddTooltip(IDC_DISPLAYPATHINHEADER, IDS_PREFTIP_UT_PATHHEADER);
	AddTooltip(IDC_LIMITCOLWIDTHS, IDS_PREFTIP_UT_LIMITCOLWIDTH);
	AddTooltip(IDC_COLWIDTHS, IDS_PREFTIP_UT_LIMITCOLWIDTH);
	AddTooltip(IDC_HIDELISTPARENTS, IDS_PREFTIP_UT_HIDELISTPARENTS);
	AddTooltip(IDC_FILELINKTHUMBNAILS, IDS_PREFTIP_UT_THUMBNAILS);
}

void CPreferencesUITasklistPage::AddSearchText(CStringArray& aText) const
{
	aText.Add(m_sDateTimeTrailText);
}

void CPreferencesUITasklistPage::OnFirstShow()
{
	CPreferencesPageBase::OnFirstShow();

	AddGroupLine(IDC_DATETIMEGROUP);
	AddGroupLine(IDC_DONEGROUP);
	AddGroupLine(IDC_OTHERGROUP);

	GetDlgItem(IDC_DISPLAYFIRSTCOMMENTLINE)->EnableWindow(m_bShowComments);
	GetDlgItem(IDC_LIMITINFOTIPCOMMENTS)->EnableWindow(m_bShowInfoTips);
	GetDlgItem(IDC_INFOTIPCOMMENTSMAX)->EnableWindow(m_bShowInfoTips && m_bLimitInfoTipCommentsLength);
	GetDlgItem(IDC_COLWIDTHS)->EnableWindow(m_bLimitColumnWidths);
	GetDlgItem(IDC_DATETIMETRAILTEXT)->EnableWindow(m_bAppendTextToDateTimePaste);
	GetDlgItem(IDC_NUMPRORITYRISKLEVELS)->EnableWindow(m_bSetNumPriorityRiskLevels);
}

int CPreferencesUITasklistPage::GetMaxInfoTipCommentsLength() const
{
	if (m_bShowInfoTips)
		return m_bLimitInfoTipCommentsLength ? max(0, m_nMaxInfoTipCommentsLength) : -1;

	return -1;
}

int CPreferencesUITasklistPage::GetMaxColumnWidth() const
{
	return m_bLimitColumnWidths ? max(0, m_nMaxColumnWidth) : -1;
}

void CPreferencesUITasklistPage::OnShowcomments() 
{
	UpdateData();

	GetDlgItem(IDC_DISPLAYFIRSTCOMMENTLINE)->EnableWindow(m_bShowComments);
}

void CPreferencesUITasklistPage::OnShowinfotips() 
{
	UpdateData();

	GetDlgItem(IDC_LIMITINFOTIPCOMMENTS)->EnableWindow(m_bShowInfoTips);
	GetDlgItem(IDC_INFOTIPCOMMENTSMAX)->EnableWindow(m_bShowInfoTips && m_bLimitInfoTipCommentsLength); 

	CPreferencesPageBase::OnControlChange();
}

void CPreferencesUITasklistPage::OnLimitinfotipcomments() 
{
	UpdateData();

	GetDlgItem(IDC_INFOTIPCOMMENTSMAX)->EnableWindow(m_bShowInfoTips && m_bLimitInfoTipCommentsLength);

	CPreferencesPageBase::OnControlChange();
}

void CPreferencesUITasklistPage::LoadPreferences(const IPreferences* pPrefs, LPCTSTR szKey)
{
	m_bShowImageTips = pPrefs->GetProfileInt(szKey, _T("ShowImageTips"), TRUE);
	m_bShowInfoTips = pPrefs->GetProfileInt(szKey, _T("ShowInfoTips"), FALSE);
	m_bShowComments = pPrefs->GetProfileInt(szKey, _T("ShowComments"), TRUE);
	m_bDisplayFirstCommentLine = pPrefs->GetProfileInt(szKey, _T("DisplayFirstCommentLine"), FALSE);
	m_bStrikethroughDone = pPrefs->GetProfileInt(szKey, _T("StrikethroughDone"), TRUE);
	m_bShowPathInHeader = pPrefs->GetProfileInt(szKey, _T("ShowPathInHeader"), TRUE);
	m_bUseISOForDates = pPrefs->GetProfileInt(szKey, _T("DisplayDatesInISO"), FALSE);
	m_bShowWeekdayInDates = pPrefs->GetProfileInt(szKey, _T("ShowWeekdayInDates"), FALSE);
	m_bShowParentsAsFolders = pPrefs->GetProfileInt(szKey, _T("ShowParentsAsFolders"), TRUE);
	m_nMaxInfoTipCommentsLength = pPrefs->GetProfileInt(szKey, _T("MaxInfoTipCommentsLength"), 100);
	m_bLimitInfoTipCommentsLength = pPrefs->GetProfileInt(szKey, _T("LimitInfoTipCommentsLength"), TRUE);
	m_bHidePercentForDoneTasks = pPrefs->GetProfileInt(szKey, _T("HidePercentForDoneTasks"), TRUE);
	m_bHideStartDueForDoneTasks = pPrefs->GetProfileInt(szKey, _T("HideStartDueForDoneTasks"), TRUE);
	m_bHideZeroTimeCost = pPrefs->GetProfileInt(szKey, _T("HideZeroTimeEst"), TRUE);
	m_bShowPercentAsProgressbar = pPrefs->GetProfileInt(szKey, _T("ShowPercentAsProgressbar"), FALSE);
	m_bRoundTimeFractions = pPrefs->GetProfileInt(szKey, _T("RoundTimeFractions"), FALSE);
	m_bUseHMSTimeFormat = pPrefs->GetProfileInt(szKey, _T("UseHMSTimeFormat"), FALSE);
	m_bAutoFocusTasklist = pPrefs->GetProfileInt(szKey, _T("AutoFocusTasklist"), FALSE);
	m_bShowColumnsOnRight = pPrefs->GetProfileInt(szKey, _T("ShowColumnsOnRight"), TRUE);
	m_bLimitColumnWidths = pPrefs->GetProfileInt(szKey, _T("LimitColumnWidths"), FALSE);
	m_nMaxColumnWidth = pPrefs->GetProfileInt(szKey, _T("MaxColumnWidth"), 60);
	m_bHideListParents = pPrefs->GetProfileInt(szKey, _T("AlwaysHideListParents"), FALSE);
	m_nPercentIncrement = pPrefs->GetProfileInt(szKey, _T("PercentIncrement"), 5);
	m_bHideZeroPercentDone = pPrefs->GetProfileInt(szKey, _T("HideZeroPercentDone"), FALSE);
	m_bAppendUserToDateTimePaste = pPrefs->GetProfileInt(szKey, _T("AppendUserToDateTimePaste"), FALSE);
	m_bAppendTextToDateTimePaste = pPrefs->GetProfileInt(szKey, _T("AppendTextToDateTimePaste"), FALSE);
	m_sDateTimeTrailText = pPrefs->GetProfileString(szKey, _T("DateTimeTrail"));
	m_bAllowCheckboxAgainstTreeItem = pPrefs->GetProfileInt(szKey, _T("AllowCheckboxAgainstTreeItem"), TRUE);
	m_bHidePaneSplitBar = pPrefs->GetProfileInt(szKey, _T("HidePaneSplitBar"), TRUE);
	m_bShowRemindersAsDateAndTime = pPrefs->GetProfileInt(szKey, _T("ShowRemindersAsDateAndTime"), FALSE);
	m_bShowFileLinkThumbnails = pPrefs->GetProfileInt(szKey, _T("ShowFileLinkThumbnails"), FALSE);
	m_bHidePriorityNumber = pPrefs->GetProfileInt(szKey, _T("HidePriorityNumber"), FALSE);
	m_bShowMixedCompletionState = pPrefs->GetProfileInt(szKey, _T("ShowMixedCompletionState"), TRUE);

	m_nNumPriorityRiskLevels = pPrefs->GetProfileInt(szKey, _T("NumPriorityRiskLevels"), TDC_PRIORITYORRISK_MAXLEVELS);
	m_bSetNumPriorityRiskLevels = (m_nNumPriorityRiskLevels < TDC_PRIORITYORRISK_MAXLEVELS);
}

void CPreferencesUITasklistPage::SavePreferences(IPreferences* pPrefs, LPCTSTR szKey) const
{
	pPrefs->WriteProfileInt(szKey, _T("ShowImageTips"), m_bShowImageTips);
	pPrefs->WriteProfileInt(szKey, _T("ShowInfoTips"), m_bShowInfoTips);
	pPrefs->WriteProfileInt(szKey, _T("ShowComments"), m_bShowComments);
	pPrefs->WriteProfileInt(szKey, _T("DisplayFirstCommentLine"), m_bDisplayFirstCommentLine);
	pPrefs->WriteProfileInt(szKey, _T("StrikethroughDone"), m_bStrikethroughDone);
	pPrefs->WriteProfileInt(szKey, _T("ShowPathInHeader"), m_bShowPathInHeader);
	pPrefs->WriteProfileInt(szKey, _T("DisplayDatesInISO"), m_bUseISOForDates);
	pPrefs->WriteProfileInt(szKey, _T("ShowWeekdayInDates"), m_bShowWeekdayInDates);
	pPrefs->WriteProfileInt(szKey, _T("ShowParentsAsFolders"), m_bShowParentsAsFolders);
	pPrefs->WriteProfileInt(szKey, _T("MaxInfoTipCommentsLength"), max(m_nMaxInfoTipCommentsLength, 0));
	pPrefs->WriteProfileInt(szKey, _T("LimitInfoTipCommentsLength"), m_bLimitInfoTipCommentsLength);
	pPrefs->WriteProfileInt(szKey, _T("HidePercentForDoneTasks"), m_bHidePercentForDoneTasks);
	pPrefs->WriteProfileInt(szKey, _T("HideStartDueForDoneTasks"), m_bHideStartDueForDoneTasks);
	pPrefs->WriteProfileInt(szKey, _T("HideZeroTimeEst"), m_bHideZeroTimeCost);
	pPrefs->WriteProfileInt(szKey, _T("ShowPercentAsProgressbar"), m_bShowPercentAsProgressbar);
	pPrefs->WriteProfileInt(szKey, _T("RoundTimeFractions"), m_bRoundTimeFractions);
	pPrefs->WriteProfileInt(szKey, _T("UseHMSTimeFormat"), m_bUseHMSTimeFormat);
	pPrefs->WriteProfileInt(szKey, _T("AutoFocusTasklist"), m_bAutoFocusTasklist);
	pPrefs->WriteProfileInt(szKey, _T("ShowColumnsOnRight"), m_bShowColumnsOnRight);
	pPrefs->WriteProfileInt(szKey, _T("LimitColumnWidths"), m_bLimitColumnWidths);
	pPrefs->WriteProfileInt(szKey, _T("MaxColumnWidth"), m_nMaxColumnWidth);
	pPrefs->WriteProfileInt(szKey, _T("AlwaysHideListParents"), m_bHideListParents);
	pPrefs->WriteProfileInt(szKey, _T("PercentIncrement"), m_nPercentIncrement);
	pPrefs->WriteProfileInt(szKey, _T("HideZeroPercentDone"), m_bHideZeroPercentDone);
	pPrefs->WriteProfileString(szKey, _T("DateTimeTrail"), m_sDateTimeTrailText);
	pPrefs->WriteProfileInt(szKey, _T("AppendUserToDateTimePaste"), m_bAppendUserToDateTimePaste);
	pPrefs->WriteProfileInt(szKey, _T("AppendTextToDateTimePaste"), m_bAppendTextToDateTimePaste);
	pPrefs->WriteProfileInt(szKey, _T("AllowCheckboxAgainstTreeItem"), m_bAllowCheckboxAgainstTreeItem);
	pPrefs->WriteProfileInt(szKey, _T("HidePaneSplitBar"), m_bHidePaneSplitBar);
	pPrefs->WriteProfileInt(szKey, _T("ShowRemindersAsDateAndTime"), m_bShowRemindersAsDateAndTime);
	pPrefs->WriteProfileInt(szKey, _T("ShowFileLinkThumbnails"), m_bShowFileLinkThumbnails);
	pPrefs->WriteProfileInt(szKey, _T("HidePriorityNumber"), m_bHidePriorityNumber);
	pPrefs->WriteProfileInt(szKey, _T("ShowMixedCompletionState"), m_bShowMixedCompletionState);
	pPrefs->WriteProfileInt(szKey, _T("NumPriorityRiskLevels"), GetNumPriorityRiskLevels());
}

int CPreferencesUITasklistPage::GetNumPriorityRiskLevels() const 
{ 
	return (m_bSetNumPriorityRiskLevels ? m_nNumPriorityRiskLevels : TDC_PRIORITYORRISK_MAXLEVELS); 
}

void CPreferencesUITasklistPage::OnLimitcolwidths() 
{
	UpdateData();

	GetDlgItem(IDC_COLWIDTHS)->EnableWindow(m_bLimitColumnWidths);

	CPreferencesPageBase::OnControlChange();
}

void CPreferencesUITasklistPage::OnShowparentsasfolders() 
{
	UpdateData();

	CPreferencesPageBase::OnControlChange();
}

void CPreferencesUITasklistPage::OnAppendTextToDateTimePaste() 
{
	UpdateData();

	GetDlgItem(IDC_DATETIMETRAILTEXT)->EnableWindow(m_bAppendTextToDateTimePaste);
}

void CPreferencesUITasklistPage::OnSetNumPriorityRiskLevels()
{
	UpdateData();

	GetDlgItem(IDC_NUMPRORITYRISKLEVELS)->EnableWindow(m_bSetNumPriorityRiskLevels);
	GetParent()->SendMessage(WM_PUITCP_NUMPRORITYRISKLEVELS);
}

void CPreferencesUITasklistPage::OnSelChangeNumPriorityRiskLevels()
{
	UpdateData();

	GetParent()->SendMessage(WM_PUITCP_NUMPRORITYRISKLEVELS);
}
