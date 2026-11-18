# UI-013: Remove the debug dead-space marker beside the horizontal scrollbar

Status: **Done** (implemented and built; TDLTest passes; visual check pending)

## Goal

When one pane of the task list gets a horizontal scrollbar and the other pane does not,
the pane without one is shortened by the scrollbar height so both panes show the same
number of rows. The strip left below the shortened pane is a normal layout gap. In Debug
builds it was painted flat dark grey, which looked like a rendering defect. Make Debug
paint it the same way as Release.

## Upstream starting point

- `CTreeListSyncer::Resize` shortens the pane without a horizontal scrollbar by
  `SM_CYHSCROLL` when the other pane has one (`Core/Shared/TreeListSyncer.cpp:3384` and
  following). This keeps the two content areas the same height for synchronised rows.
- `CTreeListSyncer::GetHScrollDeadSpace` computes the resulting strip
  (`TreeListSyncer.cpp:1416`).
- `CTreeListSyncer::HandleEraseBkgnd` paints it (`TreeListSyncer.cpp:1470`).

## What changed

File: `Core/Shared/TreeListSyncer.cpp`.

- Removed the `#ifdef _DEBUG` branch that filled the dead strip with
  `RGB(128, 128, 128)`.
- Debug and Release now both draw a themed `SBP_LOWERTRACKHORZ` scrollbar background, or
  `COLOR_SCROLLBAR` when the theme is unavailable.

## Behaviour

- The strip is still reserved; pane heights and row alignment are unchanged.
- In Debug the strip now looks like the continuation of the other pane's horizontal
  scrollbar instead of an unrelated dark grey box.

## Risks

- The dark grey fill was a deliberate Debug visual marker. Removing it means a layout
  mistake in this area is no longer obvious in Debug builds.

## Test notes

- Build: `Core/ToDoList_Core.sln` (`Unicode Debug`, `Win32`) succeeded, including the
  final `ToDoList.exe` link. `TDLTest`: 310,419/310,419 tests succeeded.
- Manual check: narrow the title pane until it gets a horizontal scrollbar while the
  attribute pane does not. Confirm the strip below the attribute pane now matches the
  scrollbar track instead of being dark grey, and that rows stay aligned.
