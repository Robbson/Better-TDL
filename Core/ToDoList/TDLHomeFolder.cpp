// TDLHomeFolder.cpp: implementation of the home folder feature
//

#include "stdafx.h"
#include "TDLHomeFolder.h"

#include "TaskFile.h"
#include "TDCCustomAttributeDef.h"
#include "TDCMsg.h"

#include "..\Interfaces\Preferences.h"
#include "..\Shared\FileMisc.h"
#include "..\Shared\Misc.h"
#include "..\3rdParty\StdioFileEx.h"

#include <afx.h>
#include <shlwapi.h>

#pragma comment(lib, "shlwapi.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////

const LPCTSTR HOMEFOLDERS_SECTION	= _T("HomeFolders");
const LPCTSTR TASKLIST_SET_EXT		= _T(".tdlset.txt");
const LPCTSTR SIZE_ATTRIB_ID		= TDC_CUSTOMATTRIB_HOMEFOLDERSIZE;
const LPCTSTR TYPE_ATTRIB_ID		= TDC_CUSTOMATTRIB_HOMEFOLDERTYPE;
const int MAX_FOLDER_DEPTH			= 32;
const int MAX_PREVIEW_FILE_SIZE		= (128 * 1024);
const COLORREF CLOSED_TDL_TEXT_COLOR = RGB(150, 150, 150);
const COLORREF OTHER_FILE_TEXT_COLOR = RGB(101, 67, 33);

static BOOL IsTextFileExtension(LPCTSTR szExtension)
{
	static LPCTSTR TEXT_EXTENSIONS[] = 
	{
		_T("txt"), _T("md"), _T("log"), _T("ini"), _T("csv"),
		_T("json"), _T("xml"), _T("yml"), _T("yaml"),
	};

	for (int nExt = 0; nExt < _countof(TEXT_EXTENSIONS); nExt++)
	{
		if (lstrcmpi(szExtension, TEXT_EXTENSIONS[nExt]) == 0)
			return TRUE;
	}

	return FALSE;
}

static BOOL IsFileInList(const CString& sFilePath, const CStringArray& aFiles)
{
	for (int nFile = 0; nFile < aFiles.GetSize(); nFile++)
	{
		if (FileMisc::IsSamePath(sFilePath, aFiles[nFile]))
			return TRUE;
	}

	return FALSE;
}

static BOOL LoadTextFile(const CString& sPath, CString& sText)
{
	CStdioFileEx file;

	if (!file.Open(sPath, (CFile::modeRead | CFile::shareDenyNone), SFEF_AUTODETECT, CP_ACP))
		return FALSE;

	if (!file.ReadFile(sText))
		return FALSE;

	// No BOM is assumed to be UTF-8, so retry as ANSI if the conversion failed
	if (sText.Find(_T('\xFFFD')) == -1)
		return TRUE;

	CStdioFileEx ansi;

	if (!ansi.Open(sPath, (CFile::modeRead | CFile::shareDenyNone), SFEF_ANSI, CP_ACP))
		return FALSE;

	return ansi.ReadFile(sText);
}

/////////////////////////////////////////////////////////////////////////////

CString CTDLHomeFolder::GetMachineKey()
{
	CString sMachine = Misc::GetComputerName();

	if (sMachine.IsEmpty())
		return _T("Default");

	CString sKey;
	
	for (int nChar = 0; nChar < sMachine.GetLength(); nChar++)
	{
		TCHAR c = sMachine[nChar];

		if (_istalnum(c) || (c == _T('-')) || (c == _T('_')) || (c == _T('.')))
			sKey += c;
		else
			sKey += _T('_');
	}

	return sKey;
}

CString CTDLHomeFolder::GetFolder(const IPreferences* pPrefs)
{
	CPreferences prefs;
	const IPreferences* pUsePrefs = (pPrefs ? pPrefs : (const IPreferences*)prefs);

	return pUsePrefs->GetProfileString(HOMEFOLDERS_SECTION, GetMachineKey());
}

BOOL CTDLHomeFolder::SetFolder(IPreferences* pPrefs, const CString& sFolder)
{
	if (!pPrefs)
		return FALSE;

	if (sFolder.IsEmpty())
		return pPrefs->DeleteProfileEntry(HOMEFOLDERS_SECTION, GetMachineKey());

	return pPrefs->WriteProfileString(HOMEFOLDERS_SECTION, GetMachineKey(), sFolder);
}

BOOL CTDLHomeFolder::HasFolder(const IPreferences* pPrefs)
{
	CString sFolder = GetFolder(pPrefs);
	return (!sFolder.IsEmpty() && FileMisc::FolderExists(sFolder));
}

BOOL CTDLHomeFolder::IsPathInHomeFolder(const CString& sFilePath)
{
	CString sFolder = GetFolder();

	if (sFolder.IsEmpty() || sFilePath.IsEmpty())
		return FALSE;

	return FileMisc::IsPathInFolder(sFilePath, sFolder);
}

CString CTDLHomeFolder::GetStoredPath(const CString& sFilePath)
{
	CString sStored(sFilePath);
	CString sFolder = GetFolder();

	if (!sFolder.IsEmpty() && FileMisc::IsPathInFolder(sStored, sFolder))
	{
		FileMisc::MakeRelativePath(sStored, sFolder, FALSE);
	}
	else
	{
		FileMisc::MakeRelativePath(sStored, FileMisc::GetAppFolder(), FALSE);
	}

	return sStored;
}

CString CTDLHomeFolder::ResolveStoredPath(const CString& sFilePath)
{
	CString sFullPath = sFilePath;

	if (::PathIsRelative(sFullPath))
	{
		CString sFolder = GetFolder();

		// Prefer the home folder but fall back to the application folder
		// for paths saved before a home folder was configured.
		if (!sFolder.IsEmpty())
		{
			CString sHomePath = FileMisc::GetFullPath(sFullPath, sFolder);

			if (FileMisc::FileExists(sHomePath) || FileMisc::FolderExists(sHomePath))
				return sHomePath;
		}

		FileMisc::MakeFullPath(sFullPath, FileMisc::GetAppFolder());
	}

	return sFullPath;
}

BOOL CTDLHomeFolder::IsFileOpen(const CString& sFilePath, const CStringArray& aOpenFiles)
{
	for (int nFile = 0; nFile < aOpenFiles.GetSize(); nFile++)
	{
		if (FileMisc::IsSamePath(sFilePath, aOpenFiles[nFile]))
			return TRUE;
	}

	return FALSE;
}

BOOL CTDLHomeFolder::IsFileModified(const CString& sFilePath, const CStringArray& aModifiedFiles)
{
	for (int nFile = 0; nFile < aModifiedFiles.GetSize(); nFile++)
	{
		if (FileMisc::IsSamePath(sFilePath, aModifiedFiles[nFile]))
			return TRUE;
	}

	return FALSE;
}

BOOL CTDLHomeFolder::IsTextFile(const CString& sFilePath)
{
	return IsTextFileExtension(FileMisc::GetExtension(sFilePath, FALSE));
}

BOOL CTDLHomeFolder::IsTasklistSetFile(const CString& sFilePath)
{
	CString sName = FileMisc::GetFileNameFromPath(sFilePath);
	int nExtLen = lstrlen(TASKLIST_SET_EXT);

	return ((sName.GetLength() > nExtLen) &&
			(sName.Right(nExtLen).CompareNoCase(TASKLIST_SET_EXT) == 0));
}

BOOL CTDLHomeFolder::ReadTasklistSet(const CString& sSetFilePath, CStringArray& aTasklistPaths)
{
	aTasklistPaths.RemoveAll();

	CString sContent;

	if (!LoadTextFile(sSetFilePath, sContent))
		return FALSE;

	CString sSetFolder = FileMisc::GetFolderFromFilePath(sSetFilePath);
	CString sHomeFolder = GetFolder();

	int nStart = 0;

	while (nStart != -1)
	{
		CString sLine = sContent.Tokenize(_T("\r\n"), nStart);

		sLine.Trim();

		// Strip a leading UTF-8 BOM and allow simple comment lines
		if (!sLine.IsEmpty() && (sLine[0] == 0xFEFF))
		{
			sLine.Delete(0);
			sLine.Trim();
		}

		if (sLine.IsEmpty() || (sLine[0] == _T('#')) || (sLine[0] == _T(';')))
			continue;

		// Strip matching quotes
		int nLength = sLine.GetLength();

		if ((nLength >= 2) &&
			(((sLine[0] == _T('"')) && (sLine[nLength - 1] == _T('"'))) ||
			 ((sLine[0] == _T('\'')) && (sLine[nLength - 1] == _T('\'')))))
		{
			sLine = sLine.Mid(1, nLength - 2);
			sLine.Trim();
		}

		if (sLine.IsEmpty())
			continue;

		CString sFullPath = sLine;

		if (::PathIsRelative(sFullPath))
		{
			// Prefer the set file's folder, then the home folder
			CString sCandidate = FileMisc::GetFullPath(sFullPath, sSetFolder);

			if (FileMisc::FileExists(sCandidate))
			{
				sFullPath = sCandidate;
			}
			else if (!sHomeFolder.IsEmpty())
			{
				CString sHomeCandidate = FileMisc::GetFullPath(sFullPath, sHomeFolder);

				if (FileMisc::FileExists(sHomeCandidate))
					sFullPath = sHomeCandidate;
				else
					sFullPath = sCandidate;
			}
			else
			{
				sFullPath = sCandidate;
			}
		}

		if (!FileMisc::HasExtension(sFullPath, _T("tdl")))
			continue;

		if (!IsFileInList(sFullPath, aTasklistPaths))
			aTasklistPaths.Add(sFullPath);
	}

	return (aTasklistPaths.GetSize() > 0);
}

BOOL CTDLHomeFolder::BuildTaskFile(CTaskFile& tasks, const CStringArray& aOpenFiles, const CStringArray& aModifiedFiles,
								   const CStringArray& aTextFiles, const CTDLHomeFolderTabColorMap& mapTabColors,
								   CTDLHomeFolderTaskColorMap& mapTaskBgColors)
{
	return BuildTaskFile(tasks, GetFolder(), aOpenFiles, aModifiedFiles, aTextFiles, mapTabColors, mapTaskBgColors);
}

BOOL CTDLHomeFolder::BuildTaskFile(CTaskFile& tasks, const CString& sFolder, const CStringArray& aOpenFiles, const CStringArray& aModifiedFiles,
								   const CStringArray& aTextFiles, const CTDLHomeFolderTabColorMap& mapTabColors,
								   CTDLHomeFolderTaskColorMap& mapTaskBgColors)
{
	tasks.Reset();
	mapTaskBgColors.RemoveAll();

	if (sFolder.IsEmpty() || !FileMisc::FolderExists(sFolder))
		return FALSE;

	tasks.SetProjectName(_T("Home"));

	CTDCCustomAttribDefinitionArray aAttribDefs;

	// Internal only: not shown as a column
	TDCCUSTOMATTRIBUTEDEFINITION fullPathDef(_T("Full Path"), FALSE);
	fullPathDef.sUniqueID = TDC_CUSTOMATTRIB_HOMEFOLDERFULLPATH;
	fullPathDef.sColumnTitle = _T("Full Path");
	fullPathDef.SetDataType(TDCCA_STRING);
	aAttribDefs.Add(fullPathDef);

	TDCCUSTOMATTRIBUTEDEFINITION sizeDef(_T("Size"), TRUE);
	sizeDef.sUniqueID = SIZE_ATTRIB_ID;
	sizeDef.sColumnTitle = _T("Size");
	sizeDef.SetDataType(TDCCA_INTEGER);
	sizeDef.dwFeatures |= TDCCAF_HIDEZERO;
	aAttribDefs.Add(sizeDef);

	TDCCUSTOMATTRIBUTEDEFINITION typeDef(_T("Type"), TRUE);
	typeDef.sUniqueID = TYPE_ATTRIB_ID;
	typeDef.sColumnTitle = _T("Type");
	typeDef.SetDataType(TDCCA_STRING);
	aAttribDefs.Add(typeDef);

	// Visible column reserved for future non-home paths
	TDCCUSTOMATTRIBUTEDEFINITION pathDef(_T("Path"), TRUE);
	pathDef.sUniqueID = TDC_CUSTOMATTRIB_HOMEFOLDERPATH;
	pathDef.sColumnTitle = _T("Path");
	pathDef.SetDataType(TDCCA_STRING);
	aAttribDefs.Add(pathDef);

	TDCCUSTOMATTRIBUTEDEFINITION statusDef(_T("Status"), TRUE);
	statusDef.sUniqueID = TDC_CUSTOMATTRIB_HOMEFOLDERSTATUS;
	statusDef.sColumnTitle = _T("Status");
	statusDef.SetDataType(TDCCA_STRING);
	aAttribDefs.Add(statusDef);

	tasks.SetCustomAttributeDefs(aAttribDefs);

	DWORD dwNextID = 1;

	return AddFolderContents(tasks, NULL, 0, sFolder, dwNextID, aOpenFiles, aModifiedFiles, aTextFiles, mapTabColors, mapTaskBgColors, 0);
}

BOOL CTDLHomeFolder::AddFolderContents(CTaskFile& tasks, void* hParent, DWORD dwParentID, const CString& sFolder,
									   DWORD& dwNextID, const CStringArray& aOpenFiles, const CStringArray& aModifiedFiles,
									   const CStringArray& aTextFiles, const CTDLHomeFolderTabColorMap& mapTabColors,
									   CTDLHomeFolderTaskColorMap& mapTaskBgColors, int nDepth)
{
	if (nDepth > MAX_FOLDER_DEPTH)
		return TRUE;

	CStringArray aFolders, aFiles;
	CFileFind finder;
	CString sWildcard = FileMisc::PathConcat(sFolder, _T("*"));

	BOOL bFound = finder.FindFile(sWildcard);

	while (bFound)
	{
		bFound = finder.FindNextFile();

		if (finder.IsDots() || finder.IsHidden() || finder.IsSystem())
			continue;

		CString sPath = finder.GetFilePath();

		if (::GetFileAttributes(sPath) & FILE_ATTRIBUTE_REPARSE_POINT)
			continue;

		if (finder.IsDirectory())
			aFolders.Add(sPath);
		else
			aFiles.Add(sPath);
	}

	finder.Close();

	Misc::SortArray(aFolders);
	Misc::SortArray(aFiles);

	HTASKITEM hParentTask = (HTASKITEM)hParent;

	for (int nFolder = 0; nFolder < aFolders.GetSize(); nFolder++)
	{
		const CString& sPath = aFolders[nFolder];
		CString sName = FileMisc::GetFileNameFromPath(sPath);
		DWORD dwTaskID = dwNextID++;

		HTASKITEM hTask = tasks.NewTask(sName, hParentTask, dwTaskID, dwParentID);

		if (!hTask)
			continue;

		tasks.SetTaskIcon(hTask, _T("0"));
		tasks.SetTaskCustomAttributeData(hTask, TDC_CUSTOMATTRIB_HOMEFOLDERFULLPATH, sPath);
		tasks.SetTaskLastModified64(hTask, FileMisc::GetFileLastModified(sPath));

		AddFolderContents(tasks, hTask, dwTaskID, sPath, dwNextID, aOpenFiles, aModifiedFiles, aTextFiles, mapTabColors, mapTaskBgColors, nDepth + 1);
	}

	for (int nFile = 0; nFile < aFiles.GetSize(); nFile++)
	{
		const CString& sPath = aFiles[nFile];
		CString sName = FileMisc::GetFileNameFromPath(sPath);
		DWORD dwTaskID = dwNextID++;

		HTASKITEM hTask = tasks.NewTask(sName, hParentTask, dwTaskID, dwParentID);

		if (!hTask)
			continue;

		tasks.SetTaskCustomAttributeData(hTask, TDC_CUSTOMATTRIB_HOMEFOLDERFULLPATH, sPath);
		tasks.SetTaskLastModified64(hTask, FileMisc::GetFileLastModified(sPath));

		CString sType = FileMisc::GetExtension(sPath, FALSE);
		BOOL bTdl = (sType.CompareNoCase(_T("tdl")) == 0);
		BOOL bTdlSet = IsTasklistSetFile(sPath);
		BOOL bText = !bTdl && !bTdlSet && (IsTextFileExtension(sType) || IsFileInList(sPath, aTextFiles));

		if (bTdlSet)
			tasks.SetTaskCustomAttributeData(hTask, TYPE_ATTRIB_ID, _T("tdlset.txt"));
		else if (!sType.IsEmpty())
			tasks.SetTaskCustomAttributeData(hTask, TYPE_ATTRIB_ID, sType);

		BOOL bOpen = FALSE;
		BOOL bModified = FALSE;

		if (bTdl)
		{
			bOpen = IsFileOpen(sPath, aOpenFiles);
			bModified = IsFileModified(sPath, aModifiedFiles);
		}
		else if (bTdlSet)
		{
			CStringArray aSetPaths;

			if (ReadTasklistSet(sPath, aSetPaths))
			{
				for (int nSetPath = 0; nSetPath < aSetPaths.GetSize(); nSetPath++)
				{
					bOpen |= IsFileOpen(aSetPaths[nSetPath], aOpenFiles);
					bModified |= IsFileModified(aSetPaths[nSetPath], aModifiedFiles);
				}
			}
		}

		if (bTdl)
		{
			tasks.SetTaskIcon(hTask, bOpen ? _T("tdl") : _T("tdlclosed"));

			COLORREF crTab = CLR_NONE;

			if (mapTabColors.Lookup(sPath, crTab) && (crTab != CLR_NONE))
				mapTaskBgColors.SetAt(dwTaskID, crTab);
		}
		else if (bTdlSet)
		{
			tasks.SetTaskIcon(hTask, _T("85"));

			if (FileMisc::GetFileSize(sPath) <= MAX_PREVIEW_FILE_SIZE)
			{
				CString sContent;

				if (LoadTextFile(sPath, sContent))
					tasks.SetTaskComments(hTask, sContent);
			}
		}
		else if (bText)
		{
			tasks.SetTaskIcon(hTask, _T("93"));

			if (FileMisc::GetFileSize(sPath) <= MAX_PREVIEW_FILE_SIZE)
			{
				CString sContent;

				if (LoadTextFile(sPath, sContent))
					tasks.SetTaskComments(hTask, sContent);
			}
		}

		CString sSize;
		sSize.Format(_T("%I64d"), (__int64)FileMisc::GetFileSize(sPath));
		tasks.SetTaskCustomAttributeData(hTask, SIZE_ATTRIB_ID, sSize);

		COLORREF crText = CLR_NONE;

		if (bOpen)
		{
			if (bModified)
			{
				crText = ::GetSysColor(COLOR_HOTLIGHT);
				tasks.SetTaskCustomAttributeData(hTask, TDC_CUSTOMATTRIB_HOMEFOLDERSTATUS, _T("Modified"));
			}
			else
			{
				tasks.SetTaskCustomAttributeData(hTask, TDC_CUSTOMATTRIB_HOMEFOLDERSTATUS, _T("Opened"));
			}
		}
		else
		{
			crText = ((bTdl || bTdlSet) ? CLOSED_TDL_TEXT_COLOR : OTHER_FILE_TEXT_COLOR);
		}

		if (crText != CLR_NONE)
			tasks.SetTaskColor(hTask, crText);
	}

	return TRUE;
}

static void CollectTasklists(const CString& sFolder, CStringArray& aPaths, int nDepth)
{
	if (nDepth > MAX_FOLDER_DEPTH)
		return;

	CStringArray aFolders;
	CFileFind finder;
	CString sWildcard = FileMisc::PathConcat(sFolder, _T("*"));

	BOOL bFound = finder.FindFile(sWildcard);

	while (bFound)
	{
		bFound = finder.FindNextFile();

		if (finder.IsDots() || finder.IsHidden() || finder.IsSystem())
			continue;

		CString sPath = finder.GetFilePath();

		if (::GetFileAttributes(sPath) & FILE_ATTRIBUTE_REPARSE_POINT)
			continue;

		if (finder.IsDirectory())
			aFolders.Add(sPath);
		else if (FileMisc::HasExtension(sPath, _T("tdl")))
			aPaths.Add(sPath);
		else if (CTDLHomeFolder::IsTasklistSetFile(sPath))
		{
			CStringArray aSetPaths;

			if (CTDLHomeFolder::ReadTasklistSet(sPath, aSetPaths))
			{
				for (int nSetPath = 0; nSetPath < aSetPaths.GetSize(); nSetPath++)
				{
					if (!IsFileInList(aSetPaths[nSetPath], aPaths))
						aPaths.Add(aSetPaths[nSetPath]);
				}
			}
		}
	}

	finder.Close();

	for (int nFolder = 0; nFolder < aFolders.GetSize(); nFolder++)
		CollectTasklists(aFolders[nFolder], aPaths, nDepth + 1);
}

void CTDLHomeFolder::CollectTasklistPaths(const CString& sFolder, CStringArray& aPaths)
{
	aPaths.RemoveAll();

	if (!sFolder.IsEmpty() && FileMisc::FolderExists(sFolder))
		CollectTasklists(sFolder, aPaths, 0);
}
