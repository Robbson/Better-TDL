# CUST-010: Auto-indent in the comments editor

Status: **In progress** (implemented and built; manual typing test pending)

## Goal

Pressing a plain `Enter` in the plain-text comments editor starts the new line with the
same leading whitespace (spaces and tabs) as the current line. On recognised bullet
lines, the marker is replaced by an equivalent run of spaces rather than repeated. In
addition, word-wrapped continuation lines are visually aligned with the first
non-whitespace character of their paragraph, so indentation remains readable when a long
line wraps.

The `Enter` behaviour restores a feature that used to be implemented as a hack in the
older comments control's `ProcessMessage(MSG*)`. The wrapped-line alignment is purely
visual; the underlying text is unchanged.

## Upstream starting point

`CTDLSimpleTextContentCtrl` derives from `CUrlRichEditCtrl`
(`Core/ToDoList/TDLSimpleTextContentCtrl.h`) and already intercepts editing keys in
`CTDLSimpleTextContentCtrl::ProcessMessage()` (`TDLSimpleTextContentCtrl.cpp:172`),
including `VK_TAB`. `CUrlRichEditCtrl` itself has no auto-indent behaviour.

## What changed

Files: `Core/ToDoList/TDLSimpleTextContentCtrl.cpp` and
`Core/ToDoList/TDLSimpleTextContentCtrl.h`.

1. A file-local `GetAutoIndentPrefix()` helper finds the start of the paragraph
   containing the caret by scanning backwards for the previous `\r`/`\n` in chunks. It
   returns the paragraph's leading spaces/tabs plus any recognised bullet marker. The
   markers are listed in `BULLET_PREFIXES` and currently include `- `, `-> `
   and `=> `; add or remove entries there to change the set. Scanning the paragraph
   rather than using `LineFromChar()`/`LineIndex()` avoids treating word-wrapped display
   lines as paragraphs.
2. `ProcessMessage()` handles `WM_KEYDOWN`/`VK_RETURN` when no Ctrl, Alt or Shift is
   held. It inserts `"\r\n"` followed by the prefix from step 1 with `ReplaceSel()` and
   returns `true` so the control does not process the key itself. On a bullet line with
   text, the marker and any extra whitespace after it are replaced by spaces, so the next
   line starts under the bullet text instead of repeating the bullet. Pressing Return on
   an empty bullet adds no bullet prefix.
3. `ApplyAutoIndent()` walks each paragraph and sets the RichEdit hanging indent
   (`PARAFORMAT2.dxOffset` / `PFM_OFFSET`) to the measured width of the paragraph's
   leading spaces plus any bullet marker. Wrapped lines therefore start where the
   paragraph's text starts. The width is measured with `PointFromChar()` and converted
   from pixels to twips, so spaces, proportional fonts, bullet markers and the control's
   custom tab stops are handled by RichEdit itself.
4. Formatting runs from a short `TIMER_AUTOINDENT` (50 ms) timer after `EN_CHANGE`,
   `WM_SETTEXT` and `WM_SETFONT`, so it is coalesced while typing and re-applied after a
   font or content change. `EM_SETPARAFORMAT` is sent directly rather than through
   `CRichEditBaseCtrl::SetParaFormat()` to avoid that helper's synthetic `EN_CHANGE`.
   The operation is wrapped in `CRePauseUndo` so the visual formatting does not create
   undo entries, and the selection/first visible line are restored.
5. Wrapped-line alignment is skipped for paragraphs whose prefix contains a tab. This
   build creates `RichEdit50W` (RichEdit 4.1, `RichEditBaseCtrl.cpp:145`), which advances
   a first-line tab from `dxStartIndent + dxOffset` but draws the line from
   `dxStartIndent`. A hanging indent on a tab-indented prefix therefore pushes the leading
   tab one whole tab stop further right (a 2-space tab renders like 3-4 tabs), and the
   measurement then folds the offset back in on every pass. Since no tab-stop layout can
   compensate for the ahead-of-line reference position, such paragraphs keep an offset of
   zero and their literal tabs.
6. While the paragraph pass runs, the caret is hidden if the control has focus, and the
   control is invalidated afterwards even when no format changed. Moving the selection to
   every paragraph with redraw disabled otherwise left non-blinking caret artefacts on
   screen (several dead `|` cursors after typing and clicking around).

## Risks

- The indent is taken from the paragraph containing the selection start. When a
  multi-line selection is replaced by Return, the new line uses the first paragraph's
  indent.
- A paragraph whose leading whitespace exceeds 1024 characters is truncated to the first
  1024 characters, which is well beyond realistic comment indentation.
- Overriding `WM_KEYDOWN` means an `Enter` used to commit an IME composition is handled
  as a normal newline; this matches the previous hack.
- The wrapped-line indent is visual only: `GetWindowText()` is still the content that is
  saved, so no spaces are added to the stored comments and paragraph formatting is lost
  when the text is re-loaded and then re-applied.
- Auto-indent is skipped for comments larger than 1 MB to avoid walking very large
  documents on every edit.
- The paragraph-format pass temporarily moves the caret to each paragraph. Selection and
  the first visible line are restored, but a horizontal scroll position is not explicitly
  restored (word wrap normally makes that irrelevant).
- Shift+Enter, Ctrl+Enter and Alt+Enter keep their existing behaviour.
- Bullet recognition requires the exact marker strings in `BULLET_PREFIXES`, including
  the trailing space. Lines without the trailing space (for example `-item`) are treated
  as ordinary text.
- Bullet markers are recognised literally, not parsed as Markdown; nested markers are
  supported only through the leading spaces/tabs before the marker.
- The Return continuation replaces each marker character with one space and each tab with
  two spaces. This aligns exactly with monospaced/coder fonts; proportional fonts may be
  off by a fraction of a character.
- Wrapped lines of tab-indented paragraphs are not aligned under the text; RichEdit 4.1
  cannot combine a hanging indent with a tabbed first line. Space-indented paragraphs
  are aligned.

## Verification

- `Core/ToDoListLib/ToDoListLib.vcxproj` builds successfully in `Unicode Debug`.
- Reproduced outside the app with a `RichEdit50W` control using the same tab stops and
  start indent: with `dxOffset = 570` (38 px) the character after a leading tab moved
  from x=29 px to x=47 px, i.e. the tab advanced from the tab stop after
  `dxStartIndent + dxOffset` instead of `dxStartIndent`. Re-applying this measured value
  each pass grew the offset by 270 twips per pass (570 → 840 → 1110 → 1380).
- With tab-containing prefixes left at offset zero, the leading tab stays at the normal
  2-character width and repeated formatting passes no longer change any paragraph.
- The caret is hidden during the paragraph pass and the control is invalidated afterwards,
  so editing and then moving the caret no longer leaves non-blinking caret artefacts.
- Manual verification pending: type an indented line in the comments editor and press
  Enter; the caret should stay at the same indent and wrapped lines of space-indented
  paragraphs should align with the text after the indent. Check bullet lines (`- item`,
  `* item`, `-> item`, `=> item`, including indented variants) for both Return
  continuation and wrapped-line alignment, plus an empty bullet followed by Return, font
  changes, paste, undo/redo and Shift+Enter.

## Automated coverage

The proposed UI automation framework ([CUST-017](CUST-017-ui-automation-framework.md))
covers this behaviour in two layers: unit tests for the prefix/bullet rules (extracted
into a testable helper) and RichEdit UIA tests that type real keystrokes and assert the
resulting text, caret and wrapped-line alignment. A UIA probe confirmed the editor
(`RICHEDIT50W`, id 1003) exposes `TextPattern` text, selection and bounding rectangles,
so these tests need no test hook. See the comments-editor section of CUST-017 for the
approach and limitations.

## Behaviour specification

Write the expectation as **text + markers**, not as prose. The same cases drive the pure
prefix/Tab logic (fast unit layer) and the real-keystroke UIA layer, so the contract cannot
drift between them.

Markers (escape with `\` when literal):

| Marker | Meaning |
|---|---|
| `^` | caret (zero-width) |
| `[` `]` | selection start / end |
| `\n` | logical line break (RichEdit returns `\r`; normalise before comparing) |
| `\t` | tab |

Current contract (decide changes explicitly here, then encode them):

| Case | Initial | Keys | Expected |
|---|---|---|---|
| Enter keeps spaces | `"  abc^"` | Enter | `"  abc\n  ^"` |
| Enter keeps a tab | `"\tabc^"` | Enter | `"\tabc\n\t^"` |
| Enter no indent | `"abc^"` | Enter | `"abc\n^"` |
| Enter continues a bullet | `"- item^"` | Enter | `"- item\n  ^"` |
| Enter indented bullet | `"  - item^"` | Enter | `"  - item\n    ^"` |
| Enter on empty bullet | `"- ^"` | Enter | `"- \n^"` |
| Enter replaces a selection | `"  abc[def\nghi]"` | Enter | `"  abc\n  ^"` |
| Shift+Enter does not indent | `"  abc^"` | Shift+Enter | `"  abc\n^"` |
| Tab inserts at the caret | `"  ab^c"` | Tab | `"  ab\t^c"` |
| Tab indents selected lines | `"[a\nb]"` | Tab | `"\t[a\n\tb]"` |
| Shift+Tab removes one tab per line | `"\t[a\n\tb]"` | Shift+Tab | `"[a\nb]"` |
| Shift+Tab without a leading tab | `"[a\nb]"` | Shift+Tab | `"[a\nb]"` |

Notes on the Tab cases: the first inserted tab is intentionally **outside** the selection;
a selection ending at a line start must not indent the following line (the implementation
trims the trailing `\r\t`). Add cases for both when the test suite is implemented.

### Wrapping and visual alignment

Wrapping is not a text transformation, so assert invariants on the rendered layout instead
of hard-coded line breaks:

1. The logical text (normalised) is unchanged by resizing or wrapping.
2. For a **space-indented** paragraph, the left x of every wrapped continuation segment
   equals the left x of the paragraph's first non-whitespace character (tolerance ±1 px).
   This is the `PARAFORMAT2.dxOffset` hanging indent in CUST-010.
3. For a **tab-indented** paragraph, continuation lines align at the paragraph start
   (`dxOffset = 0`). That is the documented RichEdit 4.1 limitation; a future fix changes
   this case deliberately.
4. With word wrap enabled there is no horizontal scrollbar for the paragraph.

Pin the environment for geometry cases: fixed frame/editor width, font (prefer a
monospaced test font), DPI and window state in the isolated INI. Derive expected positions
from font metrics (for example the width of the leading spaces) rather than committing
pixel constants. RichEdit `TextPattern` returns segment rectangles for a range; group them
by `y` to recover display lines and compare `x`. Verify that grouping in the CUST-017
Phase 0 spike before relying on it; otherwise use a narrow screenshot check for geometry
only.

### Where the cases live

Keep this table as the human-readable contract. When the tests are implemented, move the
same rows into a machine-readable fixture (TSV/JSON) read by both layers, with an optional
explicit `cpMin`/`cpMax` pair for cases where the markers are ambiguous. Do not maintain
two independent expectations.

