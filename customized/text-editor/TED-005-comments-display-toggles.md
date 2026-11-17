# TED-005: Comments editor display toggles in the main toolbar

Status: **In progress (implemented/built; interactive check pending)**

## Goal

Give the plain-text comments editor two toolbar toggles in the text-editor section of the
main toolbar:

1. **Word Wrap** on/off for the active comments editor.
2. **Show All Characters** on/off, drawing spaces and tabs as visible markers.

Word wrap already existed as a comments context-menu command; it was not reachable from
the toolbar. Show All Characters is new.

## Upstream starting point

- `CTDLSimpleTextContentCtrl` already owned `m_bWordWrap` and the `ID_COMMENTS_WORDWRAP`
  context-menu command, and saved `WordWrap` per tasklist under
  `FileStates\<tasklist>` (`Core/ToDoList/TDLSimpleTextContentCtrl.cpp`).
- `IDR_APP_TOOLBAR` already ended with the comments date/time buttons
  (`ID_COMMENTS_INSERTDATE/TIME/DATETIME`), appended by TED-001.
- `CToDoListWnd` handles the comments-specific commands and checks
  `HasFocus(TDCSF_COMMENTS)`.
- RichEdit has no built-in whitespace display, so Show All Characters is painted by the
  control itself.

## What changed

Files: `Core/ToDoList/TDLSimpleTextContentCtrl.h/.cpp`,
`Core/ToDoList/TDLCommentsCtrl.h/.cpp`, `Core/ToDoList/ToDoCtrl.h`,
`Core/ToDoList/ToDoListWnd.h/.cpp`, `Core/ToDoList/TDCMenuIconMgr.cpp`,
`Core/ToDoList/resource.h`, `Core/ToDoList/ToDoList.rc`,
`Core/ToDoList/res/app_toolbar_std.bmp`, `Core/ToDoList/res/simpletext_menu.bmp`.

1. `ID_COMMENTS_SHOWALLCHARS` (34166) was added next to the other comments commands and to
   the comments context menu below Word Wrap.
2. `CTDLSimpleTextContentCtrl` gained `m_bShowAllChars`, the `ShowAllChars` per-tasklist
   preference and the `WM_GETWORDWRAP` / `WM_SETWORDWRAP` / `WM_GETSHOWALLCHARS` /
   `WM_SETSHOWALLCHARS` messages (declared in `TDLSimpleTextContentCtrl.h`, above
   `WM_USER+1`, which the control already used for `WM_SETWORDWRAP`).
3. `OnPaint()` lets the rich edit paint normally and then draws whitespace markers with a
   client DC when `m_bShowAllChars` is set. Spaces get a centred dot and tabs get a
   right arrow; both use a mid-grey marker colour so they are visible on light and dark
   backgrounds. Only the visible character range is walked.
4. `CTDLCommentsCtrl` exposes `IsWordWrap`/`SetWordWrap`/`IsShowAllChars`/`SetShowAllChars`
   which forward the messages only when the current content format is `PLAIN_TEXT`.
   `CToDoCtrl` forwards these to the frame.
5. `CToDoListWnd` handles `ID_COMMENTS_WORDWRAP` and `ID_COMMENTS_SHOWALLCHARS` from the
   main toolbar and updates them (enable only with comments focus and simple-text content,
   check mark reflects the control's state).
6. `IDR_APP_TOOLBAR` gained both buttons after the date/time buttons and before the
   runtime-inserted comments format combo. `app_toolbar_std.bmp` was extended from 25 to
   27 images: the word-wrap image is copied from `simpletext_menu.bmp` and the
   Show All Characters image is a new pilcrow glyph. `simpletext_menu.bmp` was extended
   from 6 to 7 images for the context-menu icon.
7. `CTDCMenuIconMgr::Populate()` now maps the five comments-toolbar commands to their
   bitmap indices. This also makes the existing bitmap-width division land on 16 px
   (432 / 27) instead of 18 px (400 / 22).
8. `app_toolbar.bmp` is intentionally left unchanged, following TED-001: MFC's
   `CToolBar::LoadToolBar` requires the original 16-colour bitmap and
   `CEnToolBar::SetImage` replaces it at runtime.

## Behaviour

- The main toolbar shows Word Wrap and Show All Characters after the date/time buttons.
- Both buttons are enabled only while the comments editor has focus and the task uses the
  plain-text content format; the check marks follow the active editor.
- Word Wrap toggles the existing rich-edit wrap mode and repaints the editor.
- Show All Characters draws a dot for each space and a right arrow across each tab; the
  markers are display-only and do not change the text, the caret or the saved content.
- Both settings are stored per tasklist in `FileStates\<tasklist>` (`WordWrap`,
  `ShowAllChars`) and restored when the tasklist is opened again.
- The comments context menu offers the same two toggles and the same check marks.

## Risks

- `OnPaint` draws after the rich edit has painted. Rapid scrolling/caret blinking can
  momentarily repaint a small region without the markers until the next full paint; the
  markers are never part of the document and are not printed.
- The marker colour is a fixed mid-grey rather than a theme colour. It is visible on the
  default light background and on the dark theme, but not configurable.
- Show All Characters sends `EM_POSFROMCHAR` for each visible whitespace character.
  Documents with very many visible spaces/tabs may repaint more slowly while the option
  is enabled; the rest of the time the paint path is unchanged.
- Extending `app_toolbar_std.bmp` and `simpletext_menu.bmp` changes the image count.
  Any future toolbar/menu image must be appended and the lists in `TDCMenuIconMgr` and
  `InitMenuIconManager()` kept in the same order.
- The toggles only apply to `PLAIN_TEXT` content. Rich Text/Markdown/HTML content controls
  are unaffected and the toolbar buttons are disabled for them.

## Verification

- `msbuild Core\ToDoList_Core.sln /t:Build /p:Configuration="Unicode Debug"
  /p:Platform=Win32 /m` compiles all changed sources and resources. The final
  `ToDoList.exe` link was blocked by a running instance holding the debug executable;
  `ToDoListLib` and the resources built successfully.
- The extended bitmaps were verified byte-for-byte against the originals for the existing
  image regions; the copied word-wrap cell matches `simpletext_menu.bmp` exactly, and the
  new pilcrow cell only replaces the two new cells.
- `OnPaint()` uses the standard `Default()` + client-DC overlay technique. The workspace
  session used here has no interactive desktop (a `WM_PAINT` probe never received a
  paint), so the overlay could not be screened visually; that check remains manual.
- Manual verification pending: toggle both buttons from the main toolbar with the comments
  editor focused, check the check marks and persistence across tasklist reloads, and
  confirm that Show All Characters renders dots/arrows without changing the text.
