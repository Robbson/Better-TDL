# CUST-010: Auto-indent in the comments editor

Status: **In progress** (implemented and covered by `TDLTest`; interactive check pending)

## Goal

Pressing a plain `Enter` in the plain-text comments editor starts the new line with the
same leading whitespace (spaces and tabs) as the current line. On recognised bullet
lines, the marker is replaced by an equivalent run of spaces rather than repeated. In
addition, word-wrapped continuation lines of **space-indented** paragraphs are visually
aligned with the first non-whitespace character of their paragraph, so indentation remains
readable when a long line wraps. Tab-indented paragraphs keep their tabs and are not
hang-indented (see the options table below).

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
`Core/TDLTest/TDLSimpleTextContentCtrlTest.cpp/.h`,
`Core/TDLTest/TDLTest.cpp`, `Core/TDLTest/TDLTest.vcxproj` and `docs/TESTING.md`.

1. A file-local `GetAutoIndentPrefix()` helper finds the start of the paragraph
   containing the caret by scanning backwards for the previous `\r`/`\n` in chunks. It
   returns the paragraph's leading spaces/tabs plus any recognised bullet marker. The
   markers are listed in `BULLET_PREFIXES` and currently include `- `, `-> `
   and `=> `; add or remove entries there to change the set. Scanning the paragraph
   rather than using `LineFromChar()`/`LineIndex()` avoids treating word-wrapped display
   lines as paragraphs.
2. `ProcessEditingKey()` handles `WM_KEYDOWN`/`VK_RETURN` when no Ctrl, Alt or Shift is
   held. It inserts `"\r"` followed by the prefix from step 1 with `ReplaceSel()`, then
   restores the caret after the inserted prefix, and returns `true` so the control does
   not process the key itself. On a bullet line with text, the marker and any extra
   whitespace after it are replaced by spaces, so the next line starts under the bullet
   text instead of repeating the bullet. Pressing Return on an empty bullet adds no bullet
   prefix.
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
   undo entries, and the selection/first visible line are restored. `Tab`, `Return` and
   `Shift+Tab` call `UpdateAutoIndentNow()` after changing the text so the paragraph is
   formatted before the control repaints; otherwise a newly inserted leading tab briefly
   renders with the previous hanging indent and visibly jumps.
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
approaches were considered. This table is the decision record; the current tests cover
the chosen option and would catch a regression if another option is tried.

| Criterion | 1. Convert tabs to spaces on input | 2. Keep tabs, skip hanging indent (chosen) | 3. Hide tabs, emulate indent |
|---|---|---|---|
| Wrapped alignment, tab-indented line | Yes | **No** | Yes |
| Wrapped alignment, space-indented line | Yes | Yes | Yes |
| Literal tabs in the saved comments | No — replaced by spaces | **Yes** | **Yes** (formatting is not saved; tabs remain literal) |
| Tab key / tab stops / tab width | Tab must insert two spaces; tab stops become irrelevant for indentation; pasted/loaded tabs must be normalised too | Unchanged | Unchanged in the text; layout is driven by paragraph indents |
| Shift+Tab | Must outdent spaces (two at a time) on every path | Works; extended to the caret line and multi-line selections | Must find and account for hidden tabs; caret can sit before/after hidden characters |
| Enter auto-indent | New lines contain spaces; copied prefix tabs must be normalised | New lines keep leading tabs; bullet marker becomes spaces (as before) | New lines keep tabs; hidden state must be re-applied |
| Caret, Home/End, Backspace, selection | No hidden characters, but every insert/paste path must consistently normalise | Normal, predictable | Hidden characters can be skipped or deleted invisibly; Home/End/selection can land oddly |
| Undo/redo | Safe only if normalisation happens on input; normalising in a formatting pass creates edits that are not in the undo history | Unaffected | Character-format changes are outside plain-text undo; re-applying them can desync undo/redo |
| Saved content / other views | Tabs are lost; external tools and other views see spaces | Unchanged | Unchanged |
| 50 ms auto-indent timer | The input path can insert spaces immediately; remaining delay only for load/paste normalisation | Attribute-only pass; can be removed by formatting the caret paragraph synchronously | Formatting every pass; potential flicker/performance cost |
| Two-stage layout / visible jump | High if normalisation stays in the timer pass; low only if every insertion path is changed | **Low** | Medium (formatting still changes layout, but the text is not rewritten) |
| Implementation complexity | Low–medium: change Tab/paste/load, rewrite Shift+Tab for spaces, migrate existing content | **Low**: current implementation | **High**: hidden char formats, indentation arithmetic, caret navigation, reapply on load/font/DPI |
| Testability | Text/caret tests plus geometry | Text/caret and geometry tests implemented (offscreen `RichEdit50W`) | Needs hidden-format and interactive caret tests; hard to validate offscreen |
| Status | Tried and rejected: broke Shift+Tab and produced the tab jump | **Implemented** | Future option, not attempted |

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

### Chosen approach: 2 — keep tabs and skip the hanging indent for tab prefixes

Option 2 is chosen because it preserves the editor's native tab semantics and the stored
comments exactly, removes the text-mutating formatting pass that caused the surprising
behaviour, and is simple enough to test deterministically. The only cost is that wrapped
lines of tab-indented paragraphs are not aligned under the text. Space-indented
paragraphs, and the space-based continuation produced by Enter on a bullet line, still
align.

Option 1 was implemented first and rejected: rewriting the buffer to spaces made
`Shift+Tab` find no tabs, changed the saved comments, and combined with the timer to
produce a visible two-stage layout (a literal tab first, spaces 50 ms later). Option 3
is the only way to keep tabs *and* align every wrapped line, but it depends on hidden
character formatting and can make caret navigation and deletion around the line start
unpredictable; it remains a future option now that the behaviour is covered by tests.

### The 50 ms auto-indent timer

The timer was introduced by this customization, not by upstream ToDoList or RichEdit.
`ApplyAutoIndent()` walks every paragraph, so `ScheduleAutoIndent()` debounces it after
`EN_CHANGE`, `WM_SETTEXT` and `WM_SETFONT` to avoid moving the selection and re-measuring
the whole comments field on every keystroke. That is its only purpose.

It is not a RichEdit requirement, and it is the reason option 1 produced a visible second
layout: the pass changed the text 50 ms after the key was processed. In the chosen
approach the pass changes only paragraph attributes, and `Tab`, `Return` and `Shift+Tab`
already call `UpdateAutoIndentNow()` synchronously, so the timer is no longer needed for
an immediate editing response. The natural follow-up is to format only the caret
paragraph synchronously on `EN_CHANGE` and keep the full pass for load, font changes,
paste, undo and redo; that removes the remaining delay and the whole-document walk while
typing. Paste/undo tests should be added before doing that.

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
- Shift+Enter, Ctrl+Enter and Alt+Enter keep their existing behaviour.
- Bullet recognition requires the exact marker strings in `BULLET_PREFIXES`, including
  the trailing space. Lines without the trailing space (for example `-item`) are treated
  as ordinary text.
- Bullet markers are recognised literally, not parsed as Markdown; nested markers are
  supported only through the leading spaces/tabs before the marker.
- The Return continuation replaces each marker character with one space and each tab with
  two spaces. This aligns exactly with monospaced/coder fonts; proportional fonts may be
  off by a fraction of a character.
- The tests create the editor offscreen with a fixed size and a monospaced font. Geometry
  cases therefore do not exercise DPI changes, themes or a visible caret; those remain
  manual checks.
- The paragraph-format pass temporarily moves the caret to each paragraph. Selection and
  the first visible line are restored, but a horizontal scroll position is not explicitly
  restored (word wrap normally makes that irrelevant).
- A paragraph whose leading whitespace exceeds 1024 characters is truncated to the first
  1024 characters, which is well beyond realistic comment indentation.

## Verification

- `Core/ToDoListLib/ToDoListLib.vcxproj` builds successfully in `Unicode Debug`.
- `TDLTest` builds and passes: 310363/310363 tests, including the new
  `CTDLSimpleTextContentCtrlTest` suite (66 assertions). The suite covers every row of the
  behaviour table below, tab preservation, and space-indented/space-bullet wrapped-line
  alignment. It also asserts that tab-indented paragraphs are not re-indented (the
  `Shift+Tab` regression) and that inserting a leading tab into a `-> item` paragraph
  clears the old hanging indent immediately, so the visible tab jump cannot return.
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
- The caret is hidden during the paragraph pass and the control is invalidated afterwards,
  so editing and then moving the caret no longer leaves non-blinking caret artefacts.
- The test control uses RichEdit's CR-only character positions internally; its
  `GetWindowText` returns CRLF, so the tests read text via `GetTextRange()` for positions.
  `Return` insertion now inserts `\r` and restores the caret after the inserted indent
  explicitly, because RichEdit otherwise leaves the caret between CR and LF.
- Manual verification pending: visible editing with real keystrokes, DPI/theme changes and
  the caret artefacts check. All pure text/caret/geometry cases moved to `TDLTest` so
  further changes to the editor run them automatically.

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
[CUST-017](CUST-017-ui-automation-framework.md) remains useful for visible-caret, theme,
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
| Enter continues a bullet | `"- item^"` | Enter | `"- item\n  ^"` |
| Enter indented bullet | `"  - item^"` | Enter | `"  - item\n    ^"` |
| Enter tab-indented bullet | `"\t- item^"` | Enter | `"\t- item\n\t  ^"` |
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

Notes on the Tab cases: the first inserted tab is intentionally **outside** the selection;
a selection ending at a line start must not indent the following line (the implementation
trims the trailing `\r\t`). The `Shift+Tab` rows cover both a caret and a selection; the
outdent removes one tab or up to two leading spaces from each touched logical line.
`Shift+Enter` is asserted at the handler level: `ProcessEditingKey()` must return false so
RichEdit performs its normal line break. The other rows are encoded in `EDITKEYCASES` in
`TDLSimpleTextContentCtrlTest.cpp`.

### Wrapping and visual alignment

Wrapping is not a text transformation, so assert invariants on the rendered layout instead
of hard-coded line breaks:

1. The logical text (normalised) is unchanged by resizing or wrapping.
2. For a **space-indented** paragraph, the left x of every wrapped continuation segment
   equals the left x of the paragraph's first non-whitespace character (tolerance ±1 px).
   This is the `PARAFORMAT2.dxOffset` hanging indent in CUST-010.
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





