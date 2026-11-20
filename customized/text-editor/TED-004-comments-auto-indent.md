# TED-004: Auto-indent in the comments editor

Status: **In progress** (implemented and covered by `TDLTest`; interactive check pending)

## Goal

Pressing a plain `Enter` in the plain-text comments editor starts the new line with the
same leading whitespace (spaces and tabs) as the current line, up to its first printable
character. Bullet markers are **not** continued by `Enter`; they are only used by the
wrapped-line alignment feature. In addition, word-wrapped continuation lines of
**space-indented** paragraphs are visually aligned with the first non-whitespace character
of their paragraph, so indentation remains readable when a long line wraps. Tab-indented
paragraphs keep their tabs and are not hang-indented (see the options table below).

The `Enter` behaviour restores a feature that used to be implemented as a hack in the
older comments control's `ProcessMessage(MSG*)`. The behaviour is now specified below and
covered by `TDLTest`.

## Upstream starting point

`CTDLSimpleTextContentCtrl` derives from `CUrlRichEditCtrl`
(`Core/ToDoList/TDLSimpleTextContentCtrl.h`) and already intercepts editing keys in
`CTDLSimpleTextContentCtrl::ProcessMessage()`
(`TDLSimpleTextContentCtrl.cpp`), including `VK_TAB`. `CUrlRichEditCtrl` itself has no
auto-indent behaviour.

## What changed

Files: `Core/ToDoList/TDLSimpleTextContentCtrl.cpp` and
`Core/ToDoList/TDLSimpleTextContentCtrl.h`,
`Core/ToDoList/TDCSimpleTextContent.cpp`,
`Core/ToDoList/PreferencesUIPage.cpp/.h`,
`Core/ToDoList/ToDoList.rc`, `Core/ToDoList/resource.h`,
`Core/TDLTest/TDLSimpleTextContentCtrlTest.cpp/.h`,
`Core/TDLTest/TDLTest.cpp`, `Core/TDLTest/TDLTest.vcxproj` and `docs/TESTING.md`.

1. A file-local `GetAutoIndentPrefix()` helper finds the start of the paragraph
   containing the caret by scanning backwards for the previous `\r`/`\n` in chunks. It
   returns only the paragraph's leading spaces/tabs (up to the first printable character).
   Scanning the paragraph rather than using `LineFromChar()`/`LineIndex()` avoids treating
   word-wrapped display lines as paragraphs. Bullet markers are deliberately not returned
   here; `BULLET_PREFIXES` is used only by the wrapped-line hanging indent in step 3.
2. `ProcessEditingKey()` handles `WM_KEYDOWN`/`VK_RETURN` when no Ctrl, Alt or Shift is
   held. It inserts `"\r"` followed by the leading whitespace from step 1 with
   `ReplaceSel()`, then restores the caret after the inserted prefix, and returns `true`
   so the control does not process the key itself. This matches the usual editor
   behaviour: `"- item"` continues with no indent, `"  - item"` continues with two spaces
   and `"\t- item"` continues with a tab.
3. `ApplyAutoIndent()` walks each paragraph and sets the RichEdit hanging indent
   (`PARAFORMAT2.dxOffset` / `PFM_OFFSET`) to the measured width of the paragraph's
   leading spaces plus any recognised bullet marker. The marker list is configurable in
   **Preferences → User Interface → General → Task Comments** (defaults: `- `, `-> `,
   `=> `). Wrapped lines therefore start where the paragraph's text starts. The width is
   measured with `PointFromChar()` and converted from pixels to twips, so spaces,
   proportional fonts, bullet markers and the control's custom tab stops are handled by
   RichEdit itself. An empty list falls back to the built-in defaults.
4. Formatting is synchronous; there is no auto-indent timer. `OnChangeText()` formats only
   the caret paragraph (`ApplyAutoIndentToCaretParagraph()`), so ordinary typing does not
   walk the whole document. `Tab`, `Return` and `Shift+Tab` format only the paragraphs
   they touch (`ApplyAutoIndentRange()`), not the whole document. Content loads
   (`WM_SETTEXT`), font changes (`WM_SETFONT`), paste, dropped text/files, multi-line
   insertion, find/replace all, undo and redo run the full document pass
   (`ApplyAutoIndent()`). `EM_SETPARAFORMAT` is sent directly rather than through
   `CRichEditBaseCtrl::SetParaFormat()` to avoid that helper's synthetic `EN_CHANGE`. The
   operation is wrapped in `CRePauseUndo` so the visual formatting does not create undo
   entries. The range pass does not toggle redraw, does not invalidate the whole control,
   and does not move the selection when it is already inside the paragraph being
   formatted; this keeps typing and indentation from repainting or scrolling unrelated
   text. The selection/first visible line are restored only when a selection move was
   actually needed.
5. Pressing `Return` at the very start of a paragraph inserts only the line break. The
   original line keeps its own indent and moves down, and no copy of the prefix is added
   in front of it.
6. `Shift+Tab` removes one indent level from the line containing the caret, or from every
   logical line touched by a selection. A level is one leading tab or up to two leading
   spaces (the editor's tab width). This replaces the previous test that only looked at
   the single character immediately before the caret, which did nothing when the caret was
   elsewhere in the line.
7. Tab indentation is left in the text. Consequently the hanging-indent pass skips
   paragraphs whose prefix contains a tab. RichEdit lays out a first-line tab against the
   hanging-indent position (`dxStartIndent + dxOffset`) rather than the line's actual left
   edge, so the character after the tab lands on the continuation indent and the text
   after the marker stays marker-width to its right. Raising `dxOffset` moves both the
   continuation and the tab target by the same amount, so no offset aligns it. This keeps
   `Shift+Tab`, tab sizes and the stored content predictable.
8. While the paragraph pass runs, the caret is hidden if the control has focus, and the
   control is invalidated afterwards even when no format changed. Moving the selection to
   every paragraph with redraw disabled otherwise left non-blinking caret artefacts on
   screen (several dead `|` cursors after typing and clicking around).
9. `TDLSimpleTextContentCtrlTest` (`Core/TDLTest`) drives the real control offscreen. It
   creates a hidden host window plus a `RichEdit50W` editor with a fixed size and a
   monospaced font, calls the editing-key handler with explicit modifiers, runs the
   paragraph pass synchronously and asserts text, caret and wrapped-line positions. No
   UIA or interactive desktop is required.

## Tab-indented wrapped lines: options

RichEdit's first-line tab rule (see the explanation in the Risks section) means that a
tab-prefixed paragraph cannot be hang-indented with a literal tab in the text. Three
approaches were considered. Option 2 is the default; option 1 is now available as an
opt-in preference (see “Optional space indentation”). The current tests cover both the
default and the opt-in behaviour.

| Criterion | 1. Convert tabs to spaces on input | 2. Keep tabs, skip hanging indent (chosen) | 3. Hide tabs, emulate indent |
|---|---|---|---|
| Wrapped alignment, tab-indented line | Yes for lines indented after opting in; existing tab-indented lines remain unaligned | **No** | Yes |
| Wrapped alignment, space-indented line | Yes | Yes | Yes |
| Literal tabs in the saved comments | New indentation uses spaces; existing tabs are untouched | **Yes** | **Yes** (formatting is not saved; tabs remain literal) |
| Tab key / tab stops / tab width | Tab inserts two spaces when the option is on; existing/pasted/loaded tabs are untouched | Unchanged | Unchanged in the text; layout is driven by paragraph indents |
| Shift+Tab | Works: removes up to two leading spaces (implemented and tested) | Works; extended to the caret line and multi-line selections | Must find and account for hidden tabs; caret can sit before/after hidden characters |
| Enter auto-indent | New lines contain spaces; copied prefix tabs must be normalised | New lines copy only the leading spaces/tabs; bullet markers are not continued | New lines keep tabs; hidden state must be re-applied |
| Caret, Home/End, Backspace, selection | No hidden characters, but every insert/paste path must consistently normalise | Normal, predictable | Hidden characters can be skipped or deleted invisibly; Home/End/selection can land oddly |
| Undo/redo | Safe: spaces are inserted on input, so undo restores what was typed | Unaffected | Character-format changes are outside plain-text undo; re-applying them can desync undo/redo |
| Saved content / other views | New indentation is spaces; existing tabs stay | Unchanged | Unchanged |
| 50 ms auto-indent timer | No text-normalising timer; the input path inserts spaces immediately | **Removed**: caret paragraph formatted synchronously on edits; full pass on load/font/paste/drop/undo/redo | Formatting every pass; potential flicker/performance cost |
| Two-stage layout / visible jump | **Low**: spaces are inserted at input time; no later rewrite | **Low** | Medium (formatting still changes layout, but the text is not rewritten) |
| Implementation complexity | **Implemented** as an opt-in: Tab inserts spaces; Shift+Tab already handles spaces; existing content untouched | **Low**: current default | **High**: hidden char formats, indentation arithmetic, caret navigation, reapply on load/font/DPI |
| Testability | `SpaceIndent` text/caret tests implemented | Text/caret and geometry tests implemented (offscreen `RichEdit50W`) | Needs hidden-format and interactive caret tests; hard to validate offscreen |
| Status | **Available as an opt-in preference (default off)** | **Default** | Future option, not attempted |

### Why a first-line tab cannot be hang-indented

Let `S = dxStartIndent`, `O = dxOffset` and `M` = the width of the marker after the tab
(for example `-> `). Continuation lines start at `S + O`. RichEdit lays out the first-line
tab against `S + O` instead of the line's real left edge `S`, so the character after the
tab lands on the continuation indent; the marker then follows:

```
continuation line:                       starts at S + O
first line:        [ tab -> S + O ][ marker M ][ text at S + O + M ]
                                     ^
                                     continuation is under the marker,
                                     the text is M further right
```

Measured with Courier New 10 pt (`S` = 9 px, tab = 16 px, `-> ` = 24 px):

```
O = 0:   9        25                        49
         |--tab16--|------- "-> " ---------|text...

O = 40:  9                          49                73
         |---------tab 40 px--------|---- "-> " -----|text...
                                    ^                 ^
                                    continuation      text
                                    AND the "-" after the tab
```

Raising `O` moves the continuation and the first-line tab target by the same amount, so
the text always stays `M` to the right of the continuation. No `O` satisfies
`S + O = S + O + M`. A space-indented prefix has no tab, so its normal spaces, the
continuation indent and the text start all agree.

### Default: 2 — keep tabs and skip the hanging indent for tab prefixes

Option 2 remains the default because it preserves the editor's native tab semantics and
the stored comments exactly, removes the text-mutating formatting pass that caused the
surprising behaviour, and is simple enough to test deterministically. The only cost is
that wrapped lines of tab-indented paragraphs are not aligned under the text.
Space-indented paragraphs and space-indented bullet lines still align; Enter no longer
creates a space-based bullet continuation.

An earlier attempt to make option 1 unconditional was rejected: it rewrote the buffer
inside the formatting pass, which made `Shift+Tab` find no tabs, changed the saved
comments and produced a visible two-stage layout (a literal tab first, spaces 50 ms
later). The opt-in implementation below does none of those things because it inserts
spaces on input, never rewrites existing text and needs no timer.

Option 3 is the only way to keep tabs *and* align every wrapped line, but it depends on
hidden character formatting and can make caret navigation and deletion around the line
start unpredictable; it remains a future option now that the behaviour is covered by
tests.

### Optional space indentation (option 1, opt-in)

The user can choose to indent new lines with spaces instead of tabs under
**Preferences → User Interface → General → Task Comments → “Insert spaces instead of
tabs when indenting”**. The setting is app-wide (`Preferences\
InsertSpacesForCommentsIndent`, default `FALSE`) and is applied immediately by
`UpdateAppPreferences()`.

When enabled:

- `Tab` inserts two spaces per level (matching the editor's two-character tab stops)
  immediately, including after every line of a multi-line selection.
- A selection ending at a line start does not indent the following line.
- Existing tabs, pasted text and loaded comments are left exactly as they are; nothing is
  normalised later and no timer is involved.
- `Shift+Tab` still removes one indent level: a leading tab or up to two leading spaces.
- `Enter` copies the previous line's leading whitespace verbatim, so space-indented lines
  continue with spaces and tab-indented lines continue with tabs.
- New space-indented lines get the normal wrapped-line hanging indent and therefore align;
  old tab-indented lines remain unaligned until they are re-indented with spaces.

The `SpaceIndent` test covers single-line and multi-line Tab insertion, the
selection-ending-at-line-start case, preservation of an existing tab, and the following
`Shift+Tab` removal.

### Bullet prefix list (editable)

**Preferences → User Interface → General → Task Comments → “Bullet prefixes (one per
line)”** is a multi-line edit, modelled on the Dynamic Droplists in Default Tasklist
Attributes. Each non-blank row is one literal prefix, matched at the start of a paragraph
after its leading spaces/tabs. Prefixes include any trailing space the user wants to
match; `.INI` storage escapes the string with quotes, so trailing spaces survive a
round-trip. Defaults are `- `, `-> ` and `=> `.

The list is stored as an array under `Preferences\CommentBulletPrefixes` (`ItemCount` plus
`Item0`, `Item1`, …). A missing section or an empty saved list falls back to the built-in
defaults, so the feature cannot be left accidentally inert. `CTDLSimpleTextContentCtrl::SetBulletPrefixes()` is
called from `CTDCSimpleTextContent::LoadPreferences()` on every preference refresh, so
changes take effect without restarting. The `CustomBulletPrefixes` test replaces the list
with `* ` and checks the wrapped-line alignment, then restores the defaults.
`CPreferencesTest::CommentBulletPrefixes` loads an empty preferences file and asserts
that the preferences page and the editor both fall back to the same defaults.

### Formatting timing (implemented)

The former 50 ms `TIMER_AUTOINDENT` was introduced by this customization, not by
upstream ToDoList or RichEdit. It existed only to debounce the whole-document
`ApplyAutoIndent()` walk after `EN_CHANGE`, `WM_SETTEXT` and `WM_SETFONT`. It has been
removed:

- `OnChangeText()` formats only the caret paragraph
  (`ApplyAutoIndentToCaretParagraph()`), so ordinary typing is immediate and does not walk
  the whole comments field.
- `WM_SETTEXT`, `WM_SETFONT`, `WM_PASTE`, OLE drops (`QueryAcceptData`),
  `WM_DROPFILES`, `InsertTextContent()`, `FindReplaceAll()`, `Undo()` and `Redo()` run the
  full `ApplyAutoIndent()` pass.
- `Tab`, `Return` and `Shift+Tab` run `ApplyAutoIndentRange()` synchronously over only the
  paragraphs they touch. A single-line Tab no longer walks the whole document, so its
  response time no longer grows with the comments length.

The range pass neither toggles redraw nor invalidates the whole control, and it avoids
moving the selection when the caret is already in the paragraph being formatted. This
prevents unrelated/far-away text from repainting or the view from scrolling on every
keystroke. The full pass still uses redraw suppression and one final invalidate for bulk
changes.

This removes the delayed hanging indent that produced the visible “basic state → wrapped
state” flash when setting/loading comments, because `WM_SETTEXT` is formatted before the
control paints. The wrap *mode* is still applied through the posted
`WM_SETWORDWRAP` from `LoadPreferences()` (the old RichEdit resize workaround), so that
part is unchanged; only the paragraph-format delay is gone.

The `SynchronousFormatting` test covers load, a caret-prefix edit, multi-line insertion
and undo/redo. `FormattingScope` proves that typing, a single-line Tab, Return at a line
start and a selection Tab format only the affected paragraphs in a ten-line document.
`RepeatedIndentKeys` drives consecutive Tab/Shift+Tab keydowns and checks the intermediate
states after every call (auto-repeat).

## Risks

- The indent is taken from the paragraph containing the selection start. When a
  multi-line selection is replaced by Return, the new line uses the first paragraph's
  indent.
- Overriding `WM_KEYDOWN` means an `Enter` used to commit an IME composition is handled
  as a normal newline; this matches the previous hack.
- The wrapped-line indent is visual only and never changes the text. Wrapped lines of
  tab-indented paragraphs are **not** aligned under the first text character: RichEdit
  lays out a first-line tab against `dxStartIndent + dxOffset`, so the character after the
  tab lands on the continuation indent and the text after the marker stays marker-width
  to its right (see the options table). Replacing tabs with spaces broke `Shift+Tab` and
  made the caret jump while editing. Space-indented paragraphs are aligned; tab-indented
  paragraphs keep the normal tab width and a continuation at the paragraph start.
- `Shift+Tab` removes indentation from the caret line and from every logical line touched
  by a selection. A line starting with a tab loses the tab; otherwise up to two leading
  spaces are removed. Lines with a single leading space lose that space.
- Auto-indent is skipped for comments larger than 1 MB to avoid walking very large
  documents on every edit.
- Ordinary edits format only the caret paragraph. Any path that can change several
  paragraphs must use the full pass; the current ones are listed in “Formatting timing”.
  A future edit path that calls the rich edit directly (for example a new OLE insertion
  handler) must do the same or only its caret paragraph will be indented.
- Shift+Enter, Ctrl+Enter and Alt+Enter keep their existing behaviour.
- Bullet markers are used **only** by the wrapped-line hanging indent, not by `Enter`.
  `Enter` always copies exactly the previous line's leading whitespace.
- Bullet recognition for the hanging indent requires the exact marker strings configured
  in **Task Comments → Bullet prefixes**, including the trailing space. Lines without the
  trailing space (for example `-item`) are treated as ordinary text unless the user
  removes the space from the list entry. An empty list falls back to the built-in defaults.
- Bullet markers are recognised literally, not parsed as Markdown; nested markers are
  supported only through the leading spaces/tabs before the marker.
- `Enter` copies leading spaces and tabs verbatim (a tab stays a tab); it does not convert
  the indent to spaces.
- The optional space indentation is app-wide and affects only newly inserted indentation.
  Existing tabs, pasted text and loaded comments are not converted, so their wrapped lines
  remain unaligned until the user re-indents them with the option enabled.
- Each space-indent level is exactly two spaces, matching the control's two-character tab
  stops. The preference does not change the tab-stop width.
- The tests create the editor offscreen with a fixed size and a monospaced font. Geometry
  cases therefore do not exercise DPI changes, themes or a visible caret; those remain
  manual checks.
- The full and multi-paragraph range passes temporarily move the caret to each paragraph.
  Selection and the first visible line are restored, but a horizontal scroll position is
  not explicitly restored (word wrap normally makes that irrelevant). The single-paragraph
  caret/range path does not move the selection when the caret is already inside the
  paragraph.
- A paragraph whose leading whitespace exceeds 1024 characters is truncated to the first
  1024 characters, which is well beyond realistic comment indentation.

## Verification

- `Core/ToDoListLib/ToDoListLib.vcxproj` builds successfully in `Unicode Debug`.
- `TDLTest` builds and passes: 310436/310436 tests, including the new
  `CTDLSimpleTextContentCtrlTest` suite (126 assertions). The suite covers every row of the
  behaviour table below, tab preservation, and space-indented/space-bullet wrapped-line
  alignment. It also asserts that tab-indented paragraphs are not re-indented (the
  `Shift+Tab` regression) and that inserting a leading tab into a `-> item` paragraph
  clears the old hanging indent immediately, so the visible tab jump cannot return.
- `FormattingScope` proves the scope on a ten-line document: typing formats 1 paragraph,
  Tab with a caret formats 1, Return at a line start formats at most 2, and Tab on a
  two-line selection formats at most 3. Tab indentation therefore no longer scales with
  the total comments length.
- `RepeatedIndentKeys` presses Tab four times and then Shift+Tab four times, checking the
  text and caret after every keydown. This covers the auto-repeat case where the
  intermediate indentation states must be applied immediately, before the key is released.
- `SpaceIndent` enables the new preference and checks: two-space caret Tab, multi-line
  insertion, a selection ending at a line start, preservation of an existing tab, and
  `Shift+Tab` removing the inserted spaces. The preference control is
  `IDC_INSERTSPACESFORCOMMENTSTABS` on the UI General page under Task Comments.
- Reproduced outside the app with a `RichEdit50W` control using the same tab stops and
  start indent: for a `"\t-> "` prefix the character after the tab (`-`) moved to exactly
  `dxStartIndent + dxOffset` (the continuation indent), not to the normal tab stop after
  `dxStartIndent`. Raising the offset by one tab width moved the `-` and the continuation
  by the same one tab width. Re-applying the measured value each pass grew the offset by
  270 twips per pass (570 → 840 → 1110 → 1380).
- A sweep of `dxOffset` values showed the text always stays the marker width (the `-> `
  width) to the right of the continuation line for a tab-indented prefix, so no offset
  aligns it. Expanding the tab to spaces aligned it but broke tab-based indentation and
  `Shift+Tab`, so that approach was reverted.
- The full pass hides the caret and invalidates the control once; the single-paragraph
  range pass does neither, and it does not move the selection when the caret is already
  in the paragraph. This avoids the far-away repaint/scroll side effects on typing.
- No 50 ms auto-indent timer remains in the editor. The other application timers
  (find-highlight, mouse-cursor hiding, etc.) are unrelated. If a delayed duplicate line
  is still observed, verify that the `ToDoList.exe` binary was rebuilt after these
  changes.
- The test control uses RichEdit's CR-only character positions internally; its
  `GetWindowText` returns CRLF, so the tests read text via `GetTextRange()` for positions.
  `Return` insertion now inserts `\r` and restores the caret after the inserted indent
  explicitly, because RichEdit otherwise leaves the caret between CR and LF.
- Manual verification pending on an interactive desktop: hold Space in a long comments
  field and check that unrelated text no longer flickers; check Tab/Shift+Tab latency on a
  long document; press Enter at a line start and check that no duplicate of the moved line
  remains; hold Tab/Shift+Tab and check that each auto-repeat state is visible. The pure
  text/caret/formatting-scope cases are covered by `TDLTest`.

## Automated coverage

The automated layer is implemented in `Core/TDLTest/TDLSimpleTextContentCtrlTest.cpp`
(added to `TDLTest.vcxproj`). It creates a hidden host window and a real
`CTDLSimpleTextContentCtrl` at a fixed size with a monospaced font, then:

- drives `ProcessEditingKey()` with explicit Ctrl/Alt/Shift flags (the real
  `ProcessMessage()` passes the physical key state), so `Shift+Tab` and `Shift+Enter` are
  deterministic;
- asserts the normalised text, caret and selection using the markers below;
- runs `ApplyAutoIndent()` synchronously and asserts the wrapped continuation x positions
  using `EM_LINEINDEX` / `EM_POSFROMCHAR`.

No UIA or interactive desktop is needed. The optional UIA layer from
[GEN-003](../general/GEN-003-ui-automation-framework.md) remains useful for visible-caret, theme,
DPI and screenshot checks but is not required for the contract above.

## Behaviour specification

Write the expectation as **text + markers**, not as prose. The table below is the
human-readable contract; the same rows are encoded in `TDLTest` so the implementation and
the contract cannot drift.

Markers (escape with `\` when literal):

| Marker | Meaning |
|---|---|
| `^` | caret (zero-width) |
| `[` `]` | selection start / end |
| `\n` | logical line break (RichEdit positions use `\r`; the tests read `GetTextRange` and normalise) |
| `\t` | tab |

Current contract (decide changes explicitly here, then encode them):

| Case | Initial | Keys | Expected |
|---|---|---|---|
| Enter keeps spaces | `"  abc^"` | Enter | `"  abc\n  ^"` |
| Enter keeps a tab | `"\tabc^"` | Enter | `"\tabc\n\t^"` |
| Enter no indent | `"abc^"` | Enter | `"abc\n^"` |
| Enter after a bullet | `"- item^"` | Enter | `"- item\n^"` |
| Enter indented bullet | `"  - item^"` | Enter | `"  - item\n  ^"` |
| Enter tab-indented bullet | `"\t- item^"` | Enter | `"\t- item\n\t^"` |
| Enter on empty bullet | `"- ^"` | Enter | `"- \n^"` |
| Enter replaces a selection | `"  abc[def\nghi]"` | Enter | `"  abc\n  ^"` |
| Enter at line start (bullet) | `"^- item"` | Enter | `"\n^- item"` |
| Enter at line start (indented) | `"^  - item"` | Enter | `"\n^  - item"` |
| Enter at line start (tabbed) | `"^\t- item"` | Enter | `"\n^\t- item"` |
| Enter at line start (plain) | `"^abc"` | Enter | `"\n^abc"` |
| Shift+Enter does not indent | `"  abc^"` | Shift+Enter | `"  abc\n^"` |
| Tab inserts at the caret | `"  ab^c"` | Tab | `"  ab\t^c"` |
| Tab indents selected lines | `"[a\nb]"` | Tab | `"\t[a\n\tb]"` |
| Tab selection ends at a line start | `"[a\n]b"` | Tab | `"\t[a\n]b"` |
| Shift+Tab removes one tab per line | `"\t[a\n\tb]"` | Shift+Tab | `"[a\nb]"` |
| Shift+Tab removes the caret-line tab | `"\tabc^"` | Shift+Tab | `"abc^"` |
| Shift+Tab at a tabbed line start | `"^\tabc"` | Shift+Tab | `"^abc"` |
| Shift+Tab on a selected tabbed line | `"[\tabc]"` | Shift+Tab | `"[abc]"` |
| Shift+Tab removes two spaces | `"  abc^"` | Shift+Tab | `"abc^"` |
| Shift+Tab without a leading tab | `"[a\nb]"` | Shift+Tab | `"[a\nb]"` |

Notes on the Tab cases: the first inserted indent is intentionally **outside** the
selection; a selection ending at a line start must not indent the following line (the
implementation trims the trailing `\r` + indent). The `Shift+Tab` rows cover both a caret
and a selection; the outdent removes one tab or up to two leading spaces from each touched
logical line. When the “Insert spaces instead of tabs” preference is on, every `\t` in the
Tab rows is replaced by two spaces; `SpaceIndent` encodes those cases.
`Shift+Enter` is asserted at the handler level: `ProcessEditingKey()` must return false so
RichEdit performs its normal line break. The other rows are encoded in `EDITKEYCASES` in
`TDLSimpleTextContentCtrlTest.cpp`.

### Wrapping and visual alignment

Wrapping is not a text transformation, so assert invariants on the rendered layout instead
of hard-coded line breaks:

1. The logical text (normalised) is unchanged by resizing or wrapping.
2. For a **space-indented** paragraph, the left x of every wrapped continuation segment
   equals the left x of the paragraph's first non-whitespace character (tolerance ±1 px).
   This is the `PARAFORMAT2.dxOffset` hanging indent in TED-004.
3. For a **tab-indented** paragraph, the continuation stays at the paragraph start and the
   tab is left untouched in the text. This is the documented RichEdit 4.1 limitation and
   also what keeps `Shift+Tab` working. Tab indentation is never rewritten to spaces.
4. With word wrap enabled there is no horizontal scrollbar for the paragraph.

The tests pin the environment: a hidden host window, a 400×300 editor, Courier New 10 pt
and the control's own 120-twip start indent/two-character tab stops. Expected positions
are derived from the editor's own layout (`EM_LINEINDEX` plus `EM_POSFROMCHAR`) rather than
committed pixel constants.

### Where the cases live

`EDITKEYCASES` in `Core/TDLTest/TDLSimpleTextContentCtrlTest.cpp` is the
machine-readable fixture for the text/caret rows, and
`CTDLSimpleTextContentCtrlTest::TestWrappedLineAlignment()` encodes the geometry rules.
Update this table and those tests together.











