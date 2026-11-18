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
const LPCTSTR SIZE_ATTRIB_ID		= TDC_CUSTOMATTRIB_HOMEFOLDERSIZE;
const LPCTSTR TYPE_ATTRIB_ID		= TDC_CUSTOMATTRIB_HOMEFOLDERTYPE;
const int MAX_FOLDER_DEPTH			= 32;

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

BOOL CTDLHomeFolder::BuildTaskFile(CTaskFile& tasks, const CStringArray& aOpenFiles, const CStringArray& aModifiedFiles)
{
	return BuildTaskFile(tasks, GetFolder(), aOpenFiles, aModifiedFiles);
}

BOOL CTDLHomeFolder::BuildTaskFile(CTaskFile& tasks, const CString& sFolder, const CStringArray& aOpenFiles, const CStringArray& aModifiedFiles)
{
	tasks.Reset();

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

	tasks.SetCustomAttributeDefs(aAttribDefs);

	DWORD dwNextID = 1;

	return AddFolderContents(tasks, NULL, 0, sFolder, dwNextID, aOpenFiles, aModifiedFiles, 0);
}

BOOL CTDLHomeFolder::AddFolderContents(CTaskFile& tasks, void* hParent, DWORD dwParentID, const CString& sFolder,
									   DWORD& dwNextID, const CStringArray& aOpenFiles, const CStringArray& aModifiedFiles, int nDepth)
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

		AddFolderContents(tasks, hTask, dwTaskID, sPath, dwNextID, aOpenFiles, aModifiedFiles, nDepth + 1);
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

		if (!sType.IsEmpty())
			tasks.SetTaskCustomAttributeData(hTask, TYPE_ATTRIB_ID, sType);

		if (sType.CompareNoCase(_T("tdl")) == 0)
			tasks.SetTaskIcon(hTask, _T("tdl"));

		CString sSize;
		sSize.Format(_T("%I64d"), (__int64)FileMisc::GetFileSize(sPath));
		tasks.SetTaskCustomAttributeData(hTask, SIZE_ATTRIB_ID, sSize);

		COLORREF crText = CLR_NONE;

		if (IsFileOpen(sPath, aOpenFiles))
		{
			if (IsFileModified(sPath, aModifiedFiles))
				crText = ::GetSysColor(COLOR_HOTLIGHT);
		}
		else
		{
			crText = ::GetSysColor(COLOR_GRAYTEXT);
		}

		if (crText != CLR_NONE)
			tasks.SetTaskColor(hTask, crText);
	}

	return TRUE;
}
