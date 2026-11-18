# UI-011: Preferences dialog Apply button with Cancel rollback

Status: **Proposed** (analysed and designed; not implemented)

## Goal

Give the Preferences dialog the classic Windows settings semantics:

- An **Apply** button that is visible, disabled until something changes, and applies the
  current page values live so the user can see the effect without closing the dialog.
- **Cancel** reverts *everything* back to the state that existed before the dialog was
  opened — including changes already applied with Apply — so nothing the user did in the
  dialog survives a cancel.
- **OK** keeps existing behaviour: apply (if not already applied) and persist.

This is the documented, expected behaviour of the Modeless/Apply pattern (`PSS_APPLY`,
`PSM_CHANGED`/`PSM_SETMODIFIED` in property sheets) and matches other classic Win32 programs.

## Upstream starting point

The dialog already contains a fully wired but invisible Apply button, so most of the
plumbing exists; the missing pieces are visibility, live application outside the dialog and
rollback of applied changes.

| Item | Location | Current behaviour |
|---|---|---|
| Apply button resource | `Core/ToDoList/ToDoList.rc:90` | `PUSHBUTTON "&Apply",IDC_APPLY,...,NOT WS_VISIBLE \| WS_DISABLED` |
| Apply handler registration | `Core/ToDoList/PreferencesDlg.cpp:190` | `ON_BN_CLICKED(IDC_APPLY, OnApply)` |
| Initial disable | `Core/ToDoList/PreferencesDlg.cpp:242` | `GetDlgItem(IDC_APPLY)->EnableWindow(FALSE)` in `OnInitDialog` |
| Enable on change | `Core/ToDoList/PreferencesDlg.cpp:792` | `OnControlChange` enables Apply |
| Apply implementation | `Core/ToDoList/PreferencesDlg.cpp:783-788`, `Core/Shared/PreferencesBase.cpp:458-464`, `Core/Shared/PropertyPageHost.cpp:552-563` | `m_ppHost.OnApply()` → `pPage->UpdateData()` for each created page, then `SavePreferences` into the live `CPreferences` |
| Re-enable/disable after Apply | `Core/ToDoList/PreferencesDlg.cpp:787` | disables until the next change |
| OK path | `Core/Shared/PreferencesBase.cpp:442-456` | `CDialog::OnOK()` then `m_ppHost.OnOK()` (update + page `OnOK()`) |
| Post-dialog refresh | `Core/ToDoList/ToDoListWnd.cpp:5505-5727` (`CToDoListWnd::DoPreferences`) | The *entire* UI refresh (theme, fonts, tray, menus, tab bar, filter bar, find/reminder dialogs, tasklist prefs, custom toolbar, timers) runs only when `nRet == IDOK` |
| Persistence | `Core/ToDoList/ToDoListWnd.cpp:5536` | `CPreferences::Save()` only when `nRet == IDOK` |
| Snapshot mechanism (unused by the app) | `Core/Shared/PreferencesBase.cpp:30-` (`CTempPreferences`), `:477-489` | Only used in the `pPrefs == NULL` `DoModal` overload: snapshot before `DoModal`, reload on non-`IDOK` |
| Main app `DoModal` overload | `Core/ToDoList/PreferencesDlg.cpp:208-211` | Calls the `pPrefs != NULL` overload, so **no** snapshot/rollback is taken today |

Net effect today: the Apply button is never shown, and because page values are only read
into `CPreferences` in `OnApply`/`OnOK`, Cancel never has anything to revert. There is also
no live UI refresh while the dialog is open.

## Proposed change

### 1. Show the Apply button

- Remove `NOT WS_VISIBLE` from `IDC_APPLY` in `ToDoList.rc:90` (keep `WS_DISABLED`; the
  `OnInitDialog` `EnableWindow(FALSE)` keeps it disabled until the first change).
- Optionally re-layout the OK/Cancel/Apply row: classic order is `OK | Cancel | Apply`; the
  current template places Apply to the left (x=255) of OK/Cancel. Move it right of Cancel
  (right-align with the dialog edge minus two button widths) and update the dialog's
  `BOTTOMMARGIN`/`RIGHTMARGIN` if needed.
- `CDeferWndMove` repositioning for `IDC_APPLY` should follow the same pattern used for
  `IDOK`/`IDCANCEL` in `CPreferencesDlgBase::ReposContents` (`PreferencesBase.cpp:736` offset
  applies to Cancel today).

### 2. Make Apply drive the live UI refresh

The refresh body in `CToDoListWnd::DoPreferences` is the only place that turns a
`CPreferences` change into visible UI changes. To make Apply meaningful, that body must be
callable from inside the dialog:

- Extract `ToDoListWnd.cpp:5528-5727` (everything currently inside `if (bModified)` after
  `ResetPrefs()`) into a helper, e.g.
  `void CToDoListWnd::ApplyPreferences(const CPreferencesDlg& oldPrefs)`.
- `DoPreferences` becomes: snapshot `oldPrefs`, show dialog; on `IDOK` call
  `CPreferences::Save()` + `ApplyPreferences(oldPrefs)`; on Apply (notification from the
  dialog) call `ApplyPreferences(snapshotAtLastApply)` **without** `CPreferences::Save()`.
- The dialog needs a way to tell the main window "Apply was pressed" while it is still
  modal. Two options, prefer (a):
  - (a) Post a registered window message (e.g. `WM_PREFERENCES_APPLIED`) from
    `CPreferencesDlg::OnApply` to the owner and handle it in `CToDoListWnd`, which then
    calls `ApplyPreferences`. Keeps `ToDoListWnd` changes minimal and the dialog decoupled.
  - (b) Give `CPreferencesDlg` a `virtual void OnApplied()` hook that the app subclasses or
    that forwards via a callback set by `CToDoListWnd::DoPreferences`. More invasive to the
    shared base and ties it to the main window.
- Timers: `DoPreferences` currently calls `KillTimers()` before the dialog and
  `RestoreTimers()` after. Applying live must not re-enable timers mid-dialog, so the
  extracted helper should take a flag (e.g. `bDialogOpen`) to skip `RestoreTimers()` and
  any modal-dependent work while the dialog is open, or `RestoreTimers()` is simply left in
  `DoPreferences` after the dialog closes (preferred: keep the helper free of timer
  handling).
- Things that must **not** be applied live even if Apply is pressed: language / RTL input
  (requires restart, `CheckPromptLanguageRestart`, `ToDoListWnd.cpp:5539-5546`) and any
  option where the code currently prompts. For those, keep the check in `DoPreferences` on
  OK only, and document in the dialog / help that they take effect on OK.

### 3. Snapshot and rollback on Cancel

Rollback must undo both the in-memory `CPreferences` and the visible UI, including changes
already applied via Apply:

- In `CToDoListWnd::DoPreferences`, take a full snapshot before opening the dialog. The
  existing `const CPreferencesDlg oldPrefs` (`ToDoListWnd.cpp:5508`) is a value copy of the
  default-constructed prefs dialog and captures the pre-dialog settings; it can serve as the
  baseline, but for rollback the app needs to *restore* it, not just compare against it.
- Restoration mechanism: reuse the `CTempPreferences` approach already used by the
  `pPrefs == NULL` overload. Concretely:
  1. Before `DoModal`, `CPreferences::Save()`-equivalent the current live prefs into a
     temporary store (in-memory serialisation — `PreferencesBase.cpp:481-488` already does
     this via `SavePreferences(&prevPrefs, _T(""))`).
  2. On `IDCANCEL` (and only then), write the snapshot back into the live prefs
     (`LoadPreferences(&prevPrefs, _T(""))`) and call `ResetPrefs()` + `ApplyPreferences`
     from the restored state so the UI returns to the pre-dialog appearance.
  3. Because Apply may have already mutated the live prefs, the snapshot must be taken
     **before** the dialog opens and must be the authoritative rollback source, not the
     "state at last Apply".
- `ResetPrefs()` (`ToDoListWnd.cpp:11945`) recreates `m_pPrefs` and re-fans-out to
  `m_mgrToDoCtrls`, the filter bar, etc., so after restoring the snapshot a
  `ResetPrefs()` + `ApplyPreferences` pass should return the app to the pre-dialog look.
- Document clearly in the change file and help that Cancel means "revert everything",
  including applied changes — this is the classic Windows semantics the user asked for and
  is the main behavioural deviation from a plain OK/Cancel dialog.

### 4. Edge cases to handle

- **Apply enabled state**: Apply disables after a successful apply (`PreferencesDlg.cpp:787`)
  and re-enables on the next control change (`:792`). Because `m_ppHost.OnApply()` calls
  `UpdateData()` on all pages, any DDX-based value change also fires `OnControlChange`
  (control-enable/disable handlers), so keep the existing disable-after-apply sequence.
- **Apply with nothing changed**: should be a no-op (button disabled). The current
  enable-on-any-`EN_CHANGE` handler also fires for programmatic edits (page initialisation);
  `OnInitDialog` already forces disabled, but per-page `LoadPreferences` called on the
  snapshot path must not re-enable it. Verify by toggling the search box and pages.
- **Pages whose settings prompt** (e.g. changing the tasklist path or default task
  attributes) must be safe to run twice (Apply then OK). `m_ppHost.OnOK()` runs
  `UpdateData()` + `OnOK()` on all pages; Apply runs only `UpdateData()`. The page `OnOK()`
  virtual (`PreferencesBase.h`) should ideally contain only validation, not side effects;
  audit each `Preferences*Page::OnOK()` for non-idempotent work before enabling Apply.
- **Language / RTL**: do not apply live; keep the restart prompt on OK only.
- **Timers / modal-only work**: keep `KillTimers`/`RestoreTimers` around the whole dialog;
  do not restart timers from the Apply helper.
- **Persisted state vs in-memory state**: Apply must change the running app but not the INI.
  `CPreferences::Save()` must stay on OK only. On Cancel after Apply, the restored in-memory
  state must also be what later `CPreferences::Save()` calls (e.g. on exit) write, so the
  INI never gets the applied-but-cancelled values. Confirm `CPreferences::Save()` serialises
  the live object, so restoring the live object before exit is sufficient.
- **Re-entrancy**: pressing Apply while a page is validating, or pressing OK while Apply is
  running, must not re-enter `ApplyPreferences`. Guard with a flag or disable OK/Cancel
  during apply.
- **Visible UI side effects that cannot be reverted** (e.g. global hotkey registration,
  clicking "Clear MRU", file-extension/protocol registration at
  `ToDoListWnd.cpp:5572-5576`): decide per control whether "Apply" is allowed or whether the
  setting is OK-only. Anything with external state must be OK-only or made reversible.
- **Plugin-specific preference dialogs** (`Core/*Ext/*PreferencesDlg.cpp`,
  `Core/RTFContentCtrl`, `KanbanBoard`) use `CPreferencesDlgBase` with a different resource
  (`IDD_PREFERENCES_DIALOG`). Keeping the Apply change confined to the main app's
  `IDD_PREFERENCES` avoids changing plugin dialogs; if the base-class hook (option 2b) is
  chosen, verify plugin dialogs still compile and behave.

## Files likely touched

- `Core/ToDoList/ToDoList.rc` — Apply button visibility/layout for `IDD_PREFERENCES`.
- `Core/ToDoList/PreferencesDlg.cpp` — `OnApply` posts an "applied" notification; optional
  layout in `ReposContents`-driven resize.
- `Core/ToDoList/ToDoListWnd.cpp` — extract `ApplyPreferences(oldPrefs)` from
  `DoPreferences`; snapshot before `DoModal`; handle apply notification; rollback on
  `IDCANCEL`.
- `Core/ToDoList/ToDoListWnd.h` — declare `ApplyPreferences` and the notification handler.
- `Core/Shared/PreferencesBase.h/.cpp` *(only if option 2b or a shared snapshot helper is
  used)* — expose snapshot/restore or an `OnApplied` virtual.
- `customized/index.md` — add the UI-011 row.

## Risks

- The post-dialog refresh is large (`ToDoListWnd.cpp:5528-5727`) and touches many managers.
  Running it repeatedly must be idempotent; some operations (rebuilding the custom toolbar,
  reloading menus, re-registering hovers) may be expensive on every Apply. Measure or
  debounce if Apply feels slow.
- Some refreshes are destructive (recreating fonts, destroying/recreating the custom
  toolbar). Applying live may briefly flicker; acceptable but to be verified.
- Rollback correctness depends on the snapshot covering *every* preference the pages write.
  Any page that writes a value outside its `SavePreferences` (e.g. directly to the registry
  or a side channel) will not be reverted. Audit each `Preferences*Page::SavePreferences`.
- The user-facing semantics ("Cancel reverts even what I applied") differ from a simple
  dialog; make sure it is documented so it is not reported as a bug.
- `IDC_APPLY` and `IDC_APPLYASFILTER` share the same numeric value (`1057`,
  `Core/ToDoList/resource.h:309-311`). They are never in the same dialog, so this is safe,
  but be careful when editing `resource.h`.

## Test notes (to fill in when implemented)

- Build `Core/ToDoList_Core.sln` (`Unicode Debug`, `Win32`).
- Manual: open Preferences, change a visible setting (e.g. dates in ISO), press Apply →
  view updates immediately, Apply disables; press Cancel → view returns to pre-dialog state.
- Manual: change setting → OK → setting persists to INI (`Tools > Preferences` after
  restart retains it).
- Manual: change setting → Apply → Cancel → setting is reverted and **not** written on exit
  (`ToDoList.ini` unchanged).
- Manual: change language → OK shows the restart prompt; Apply does not apply language.
- Manual: open Preferences repeatedly and verify Apply is disabled on open, enabling only on
  a genuine change; verify each page (all 16) can round-trip Apply → OK.
- Confirm plugin preference dialogs still open and work unchanged.

## Related

- [UI-010](UI-010-preferences-tooltips.md) — Preferences dialog tooltips (shares
  `CPreferencesPageBase` and `Preferences*Page` implementations; retest tooltips after
  touching the pages).
- Back to [customized/index.md](../index.md).
