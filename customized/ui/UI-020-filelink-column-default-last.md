# UI-020 — File Link column defaults to last

## Goal

Change the default physical order of the tasklist attribute columns so that the
**File Link** column is the last attribute column. Upstream places `TDCC_FILELINK`
early in the default column sequence, between **Depends** and **Flag**.

This only changes the default for new tasklists and for tasklists with no stored column
order. A user's existing saved column order is preserved and can still be changed by
dragging column headers or with **View > Select Columns**.

## Upstream starting point

The initial tasklist column order is not persisted in a preference; it is the order of the
`COLUMNS[]` table in `Core/ToDoList/TDCStatic.h`. `CTDLTaskCtrlBase::BuildColumns()`
creates the header items in that order, and only an explicitly saved
`<tasklist>\ColumnOrder` array reorders them on `LoadState()`.

## Implementation

Moved the `TDCC_FILELINK` entry in `COLUMNS[]` from between `TDCC_DEPENDENCY` and
`TDCC_FLAG` to just before the special `TDCC_CLIENT` column, i.e. after
`TDCC_COMMENTSFORMAT`. This makes File Link the last attribute column in the default
order; because it is also visible by default it is the last visible column.

The `TDC_COLUMN` enum order in `Core/ToDoList/TDCEnum.h` is intentionally unchanged
(the repository keeps that enum stable). The **Select Columns** dialog keeps its own
list order, which does not affect the tasklist display order.

## Files

- `Core/ToDoList/TDCStatic.h` — reordered the `TDCC_FILELINK` entry in `COLUMNS[]`.

## Risks

- `COLUMNS[]` also drives the order of the Group By dropdown and the multi-sort dialog.
  File Link already appears there; it now appears last. No functional change.
- Users with an existing saved column order see no change until they reset the order,
  because their order is restored from preferences.

## Verification

- `msbuild Core\ToDoList_Core.sln /t:Build /p:Configuration="Unicode Debug"
  /p:Platform=Win32 /m` succeeded with 0 errors.
- `Core\TDLTest\Unicode_Debug\TDLTest.exe` passed `310436/310436` tests.
- Open a new tasklist with no saved column order and confirm File Link is the last
  attribute column (before any custom columns).
