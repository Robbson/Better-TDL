// PreferencesTest.cpp: implementation of the CPreferencesTest class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "TDLTest.h"
#include "PreferencesTest.h"

#include "..\shared\misc.h"
#include "..\shared\filemisc.h"

#include "..\ToDoList\PreferencesSearchIndex.h"

#include "..\interfaces\preferences.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CPreferencesTest::CPreferencesTest(const CTestUtils& utils) 
	: 
	CTDLTestBase(_T("CPreferencesTest"), utils)
{

}

CPreferencesTest::~CPreferencesTest()
{

}

TESTRESULT CPreferencesTest::Run()
{
	ClearTotals();

	TestQuotedStrings();
	TestSearchIndexDialogTemplates();
	TestSearchIndexMatching();
	TestArrayPerformance();
	TestSectionPerformance();

	return GetTotals();
}

void CPreferencesTest::TestQuotedStrings()
{
	CTDCScopedTest test(*this, _T("CPreferencesTest::QuotedStrings"));

	CString sIniPath = FileMisc::GetTempFilePath(_T("TestQuotedStrings"), _T(".ini"));
	FileMisc::DeleteFile(sIniPath);

	CStringArray aValues;
	aValues.Add(_T("\"Jardy Glaube\""));
	aValues.Add(_T("say \"hello\" now"));
	aValues.Add(_T("\""));
	aValues.Add(_T("no quotes"));

	// Write and save
	{
		CPreferences::Initialise(sIniPath, TRUE);

		// scoped so that preferences goes out of scope before release
		{
			CPreferences prefs;

			for (int nItem = 0; nItem < aValues.GetSize(); nItem++)
			{
				CString sKey = Misc::MakeKey(_T("Item%d"), nItem);
				prefs.WriteProfileString(_T("QuotedStrings"), sKey, aValues[nItem]);
			}

			prefs.WriteProfileArray(_T("QuotedArray"), aValues);
			prefs.Save();
		}

		CPreferences::Release();
	}

	// Read back
	{
		CPreferences::Initialise(sIniPath, TRUE);

		{
			CPreferences prefs;

			for (int nItem = 0; nItem < aValues.GetSize(); nItem++)
			{
				CString sKey = Misc::MakeKey(_T("Item%d"), nItem);
				CString sValue = prefs.GetProfileString(_T("QuotedStrings"), sKey);

				ExpectEQ(sValue, aValues[nItem]);
			}

			CStringArray aReloaded;
			prefs.GetProfileArray(_T("QuotedArray"), aReloaded, TRUE);

			ExpectEQ(aReloaded, aValues, TRUE);
		}

		CPreferences::Release();
	}

	FileMisc::DeleteFile(sIniPath);
}

void CPreferencesTest::TestSectionPerformance()
{
	if (!m_utils.GetWantPerformanceTests())
	{
		_tprintf(_T("Add '-p' to run CPreferencesTest::SectionPerformance\n"));
		return;
	}

	CTDCScopedTest test(*this, _T("CPreferencesTest::SectionPerformance"));

	CString sIniPath = FileMisc::GetTempFilePath(_T("Test"), _T(".ini"));

	FileMisc::DeleteFile(sIniPath);
	CPreferences::Initialise(sIniPath, TRUE);

	CPreferences prefs;
	int nNumSection = 1000, nNumEntry = 50;

	// Writing
	{
		DWORD dwTickStart = GetTickCount();

		for (int k = 0; k < 10; k++)
		{
			for (int nEntry = 0; nEntry < nNumEntry; nEntry++)
			{
				CString sEntryKey = Misc::MakeKey(_T("Entry%d"), nEntry);

				for (int nSection = 0; nSection < nNumSection; nSection++)
				{
					CString sSectionKey = Misc::MakeKey(_T("Section%d"), nSection);

					prefs.WriteProfileInt(sSectionKey, sEntryKey, nSection * nEntry);
				}
			}
		}

		DWORD dwDuration = (GetTickCount() - dwTickStart);
		_tprintf(_T("Writing %d sections x %d entries took %ld ms\n"), nNumSection, nNumEntry, dwDuration / 10);
	}

	// Reading
	{
		DWORD dwTickStart = GetTickCount();
		int nSum = 0;

		for (int k = 0; k < 10; k++)
		{
			for (int nEntry = 0; nEntry < nNumEntry; nEntry++)
			{
				CString sEntryKey = Misc::MakeKey(_T("Entry%d"), nEntry);

				for (int nSection = 0; nSection < nNumSection; nSection++)
				{
					CString sSectionKey = Misc::MakeKey(_T("Section%d"), nSection);

					nSum += prefs.GetProfileInt(sSectionKey, sEntryKey, nSection * nEntry);
				}
			}
		}

		DWORD dwDuration = (GetTickCount() - dwTickStart);
		_tprintf(_T("Reading %d sections x %d entries took %ld ms\n"), nNumSection, nNumEntry, dwDuration / 10);
	}

#ifndef _DEBUG
	// Reset preferences to prevent a save affecting timing
	CPreferences::Release();
#endif
}

void CPreferencesTest::TestSearchIndexDialogTemplates()
{
	CTDCScopedTest test(*this, _T("CPreferencesSearchIndex::DialogTemplates"));

	CString sExe = GetToDoListExePath();

	if (!FileMisc::FileExists(sExe))
	{
		_tprintf(_T("    Skipped: '%s' not found\n"), (LPCTSTR)sExe);
		return;
	}

	HMODULE hMod = ::LoadLibraryEx(sExe, NULL, LOAD_LIBRARY_AS_DATAFILE);

	if (!ExpectTrue(hMod != NULL))
		return;

	ExpectTrue(ContainsTemplateText(hMod, 139, _T("&Home Folder")));       // IDD_PREFGEN_PAGE
	ExpectTrue(ContainsTemplateText(hMod, 158, _T("&Preview results")));  // IDD_PREFEXPORT_PAGE
	ExpectTrue(ContainsTemplateText(hMod, 157, _T("&Tasklist font")));    // IDD_PREFUITASKLISTCOLORS_PAGE
	ExpectTrue(ContainsTemplateText(hMod, 142, _T("&Show menu item IDs"))); // IDD_PREFSHORTCUTS_PAGE (DIALOGEX)

	::FreeLibrary(hMod);
}

void CPreferencesTest::TestSearchIndexMatching()
{
	CTDCScopedTest test(*this, _T("CPreferencesSearchIndex::Matching"));

	CPreferencesSearchIndex index;
	index.SetPageCount(3);

	index.AddText(0, _T("Home Folder"));
	index.AddText(1, _T("Tasklist font"));
	index.AddText(1, _T("Tahoma"));
	index.SetAlwaysMatches(2);

	CStringArray aTerms;
	aTerms.Add(_T("FOLDER"));

	ExpectTrue(index.PageMatches(0, aTerms));
	ExpectFalse(index.PageMatches(1, aTerms));
	ExpectTrue(index.PageMatches(2, aTerms));

	aTerms.RemoveAll();
	aTerms.Add(_T("tahoma"));

	ExpectTrue(index.PageMatches(1, aTerms));

	aTerms.RemoveAll();
	aTerms.Add(_T("nomatch"));

	ExpectFalse(index.PageMatches(0, aTerms));
	ExpectFalse(index.PageMatches(1, aTerms));
}

BOOL CPreferencesTest::ContainsTemplateText(HMODULE hMod, int nID, LPCTSTR szExpected)
{
	HRSRC hRes = ::FindResource(hMod, MAKEINTRESOURCE(nID), RT_DIALOG);

	if (!hRes)
		return FALSE;

	HGLOBAL hMem = ::LoadResource(hMod, hRes);

	if (!hMem)
		return FALSE;

	const DLGTEMPLATE* pTemplate = reinterpret_cast<const DLGTEMPLATE*>(::LockResource(hMem));

	if (!pTemplate)
		return FALSE;

	CStringArray aText;

	if (!CPreferencesSearchIndex::ParseDialogTemplate(pTemplate, aText))
		return FALSE;

	for (int nText = 0; nText < aText.GetSize(); nText++)
	{
		if (aText[nText] == szExpected)
			return TRUE;
	}

	return FALSE;
}

CString CPreferencesTest::GetToDoListExePath()
{
	CString sConfig(FileMisc::GetFileNameFromPath(FileMisc::GetAppFolder()));
	CString sExe = FileMisc::GetAppFolder();

	sExe += _T("\\..\\..\\ToDoList\\");
	sExe += sConfig;
	sExe += _T("\\ToDoList.exe");

	return FileMisc::GetFullPath(sExe);
}

void CPreferencesTest::TestArrayPerformance()
{
	if (!m_utils.GetWantPerformanceTests())
	{
		_tprintf(_T("Add '-p' to run CPreferencesTest::TestArrayPerformance\n"));
		return;
	}

	CTDCScopedTest test(*this, _T("CPreferencesTest::TestArrayPerformance"));

	// Separate items
	{
		CString sIniPath = FileMisc::GetTempFilePath(_T("TestSeparateArrayItems"), _T(".ini"));
		int nTestSize = 100;

		for (int i = 0; i < 3; i++)
		{
			FileMisc::DeleteFile(sIniPath);
			CPreferences::Initialise(sIniPath, TRUE);

			// scoped so that preferences goes out of scope before release
			{
				CPreferences prefs;

				CDWordArray aItems;
				InitArray(aItems, nTestSize);

				// Write test
				{

					DWORD dwTickStart = GetTickCount();

					for (int k = 0; k < 10; k++)
					{
						for (int i = 0; i < nTestSize; i++)
						{
							prefs.WriteProfileString(_T("Preferences\\SeparateItems"), Misc::MakeKey(_T("Item%d"), i), Misc::Format(aItems[i]));
						}
					}

					DWORD dwDuration = (GetTickCount() - dwTickStart);
					_tprintf(_T("Writing %d separate items took %ld ms\n"), nTestSize, dwDuration / 10);
				}

				// Read test
				{
					DWORD dwTickStart = GetTickCount();

					for (int k = 0; k < 10; k++)
					{
						for (int i = 0; i < nTestSize; i++)
						{
							CString sItem = prefs.GetProfileString(_T("Preferences\\SeparateItems"), Misc::MakeKey(_T("Item%d"), i));
						}
					}

					DWORD dwDuration = (GetTickCount() - dwTickStart);
					_tprintf(_T("Reading %d separate items took %ld ms\n"), nTestSize, dwDuration / 10);
				}

				// Saving
				{
					DWORD dwTickStart = GetTickCount();

					for (int k = 0; k < 10; k++)
					{
						// Have to change something to cause it to save again
						prefs.WriteProfileInt(_T("Preferences"), _T("Test"), k);

						prefs.Save();
					}

					DWORD dwDuration = (GetTickCount() - dwTickStart);
					_tprintf(_T("Saving %d separate items took %ld ms\n"), nTestSize, dwDuration / 10);
				}
			}

			CPreferences::Release();

			nTestSize *= 10;
		}

	}

	// Single concatenated item
	{
		CString sIniPath = FileMisc::GetTempFilePath(_T("TestSingleArrayItem"), _T(".ini"));
		int nTestSize = 100;

		for (int i = 0; i < 3; i++)
		{
			FileMisc::DeleteFile(sIniPath);
			CPreferences::Initialise(sIniPath, TRUE);

			// scoped so that preferences goes out of scope before release
			{
				CPreferences prefs;

				CDWordArray aItems;
				InitArray(aItems, nTestSize);

				// Write test
				{
					DWORD dwTickStart = GetTickCount();

					for (int k = 0; k < 10; k++)
						prefs.WriteProfileString(_T("Preferences"), _T("SingleItem"), Misc::FormatArray(aItems, ','));

					DWORD dwDuration = (GetTickCount() - dwTickStart);
					_tprintf(_T("Writing single %d item took %ld ms\n"), nTestSize, dwDuration / 10);
				}

				// Read test
				{
					DWORD dwTickStart = GetTickCount();

					for (int k = 0; k < 10; k++)
					{
						CString sItem = prefs.GetProfileString(_T("Preferences"), _T("SingleItem"));
						Misc::Split(sItem, aItems, ',');
					}

					DWORD dwDuration = (GetTickCount() - dwTickStart);
					_tprintf(_T("Reading single %d item took %ld ms\n"), nTestSize, dwDuration / 10);
				}

				// Saving
				{
					DWORD dwTickStart = GetTickCount();

					for (int k = 0; k < 10; k++)
					{
						// Have to change something to cause it to save again
						prefs.WriteProfileInt(_T("Preferences"), _T("Test"), k);

						prefs.Save();
					}

					DWORD dwDuration = (GetTickCount() - dwTickStart);
					_tprintf(_T("Saving single %d item took %ld ms\n"), nTestSize, dwDuration / 10);
				}

			}

			CPreferences::Release();

			nTestSize *= 10;
		}
	}
}

void CPreferencesTest::InitArray(CDWordArray& aItems, int nSize)
{
	aItems.RemoveAll();

	for (int i = 0; i < nSize; i++)
		aItems.Add(i);
}
