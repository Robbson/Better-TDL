// TDLHomeFolder.h: interface for the home folder feature
//

#if !defined(AFX_TDLHOMEFOLDER_H__5C1F6A52_1F7B_4D0E_9C4E_2B2D5A7B3C11__INCLUDED_)
#define AFX_TDLHOMEFOLDER_H__5C1F6A52_1F7B_4D0E_9C4E_2B2D5A7B3C11__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include <afxtempl.h>

class CTaskFile;
class IPreferences;

/////////////////////////////////////////////////////////////////////////////

class CTDLHomeFolder
{
public:
	static CString GetFolder(const IPreferences* pPrefs = NULL);
	static BOOL SetFolder(IPreferences* pPrefs, const CString& sFolder);
	static BOOL HasFolder(const IPreferences* pPrefs = NULL);

	static CString GetMachineKey();

	static BOOL IsPathInHomeFolder(const CString& sFilePath);
	static CString GetStoredPath(const CString& sFilePath);
	static CString ResolveStoredPath(const CString& sFilePath);

	static BOOL BuildTaskFile(CTaskFile& tasks, const CStringArray& aOpenFiles, const CStringArray& aModifiedFiles);
	static BOOL BuildTaskFile(CTaskFile& tasks, const CString& sFolder, const CStringArray& aOpenFiles, const CStringArray& aModifiedFiles);

	static void CollectTasklistPaths(const CString& sFolder, CStringArray& aPaths);

protected:
	static BOOL AddFolderContents(CTaskFile& tasks, void* hParent, DWORD dwParentID, const CString& sFolder,
								  DWORD& dwNextID, const CStringArray& aOpenFiles, const CStringArray& aModifiedFiles, int nDepth);
	static BOOL IsFileOpen(const CString& sFilePath, const CStringArray& aOpenFiles);
	static BOOL IsFileModified(const CString& sFilePath, const CStringArray& aModifiedFiles);
};

/////////////////////////////////////////////////////////////////////////////

#endif // !defined(AFX_TDLHOMEFOLDER_H__5C1F6A52_1F7B_4D0E_9C4E_2B2D5A7B3C11__INCLUDED_)
