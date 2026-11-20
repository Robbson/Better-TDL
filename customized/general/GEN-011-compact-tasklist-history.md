# GEN-011 — Compact tasklist backups and per-task history

**Status:** Proposed (design and measurements; no implementation)

**Scope:** Backup creation and a new optional history store. No change to the `.tdl`
format, task IDs, `TDC_ATTRIBUTE`/`TDC_COLUMN` order or the source-control protocol.

**Related:** [index.md](../index.md), [../../docs/ARCHITECTURE.md](../../docs/ARCHITECTURE.md),
[../../docs/DATA_MODEL.md](../../docs/DATA_MODEL.md), [GEN-002](GEN-002-quoted-ini-values.md),
[GEN-010](GEN-010-shared-ini-session-state.md)

## Goal

Reduce the disk cost of tasklist backups and make it possible to answer:

- "What did this tasklist look like yesterday / last week?"
- "Which tasks changed on 18 November?"
- "What changed on task 42?"

Today the app keeps up to N **complete** `.tdl` copies. That is safe and simple but
wasteful: a 1.7 MB tasklist costs ~85 MB for 50 backups. The proposal is to keep the
existing full-copy backup as the safety net, add optional compression immediately, and
then add a structural, per-task history store that stores only changed task records and
changed attributes.

## Upstream starting point

### Current backup mechanism

- On save, `CToDoListWnd::DoSaveWithBackupAndProgress` calls
  `CToDoCtrlMgr::DoBackup` before writing (`Core/ToDoList/ToDoListWnd.cpp:2266-2275`).
  The same happens before the end-session save (`ToDoListWnd.cpp:9921-9924`).
- `CToDoCtrlMgr::DoBackup` resolves the folder and keeper count and calls
  `CTDCFileBackup::CreateBackup` (`Core/ToDoList/ToDoCtrlMgr.cpp:1608-1631`).
- `CreateBackup` makes a full copy with `CFileBackup::MakeBackup` using
  `FBS_APPVERSION | FBS_DATETIMESTAMP` (`Core/ToDoList/TDCFileBackup.cpp:51-66`), then
  culls old files (`TDCFileBackup.cpp:68-113`).
- Culling keeps `N` copies for the current application version and only **one** copy for
  each older version, deliberately preserving pre-upgrade copies (`TDCFileBackup.cpp:92-112`).
- Preferences default to `BackupOnSave=TRUE`, `BackupLocation=backup` relative to the
  tasklist and `KeepBackups=10` (`Core/ToDoList/PreferencesFile2Page.cpp:239-244`). The
  combo offers 1, 2, 3, 4, 5, 10, 20, 50 and All (`Core/ToDoList/ToDoList.rc:1940-1963`).
- `CToDoCtrl::SaveTaskfile` additionally creates a temporary same-folder backup before
  every write and restores it on failure (`Core/ToDoList/ToDoCtrl.cpp:4464-4484`). This is
  crash protection, not long-term history.

### Existing version-like data

- `FILEVERSION` is incremented on each modified save (`Core/ToDoList/ToDoCtrl.cpp:4444-4446`)
  and used only as a coarse lost-update check for network files: if the on-disk version is
  greater than the loaded version, the user is prompted before overwriting
  (`ToDoCtrl.cpp:4422-4441`). It is one integer, not a history.
- Each task stores `LASTMOD` and `LASTMODBY`
  (`Core/Interfaces/TasklistSchemaDef.h:93,116`; accessors in `Core/ToDoList/TaskFile.h:180-198`).
  This is only the latest modification time. Once the file is overwritten, older values
  are gone.
- Source control is a `CHECKEDOUTTO` lock in the XML header, not a repository
  (`Core/Interfaces/TasklistSchemaDef.h:16`, `Core/ToDoList/TDCSourceControl.cpp:104-268`).
  It prevents concurrent writes and therefore helps history consistency, but provides no
  history itself.

## Findings that constrain the design

### The XML is well suited to per-task diffs

- Tasks are nested `<TASK>` elements with stable `DWORD` IDs (`ID`, `POS`, `PARENTID`;
  `TasklistSchemaDef.h:56-62`). Handles are mapped by ID
  (`Core/ToDoList/TaskFile.cpp:835-861`) and new tasks receive IDs from `NEXTUNIQUEID`
  (`TaskFile.cpp:2486`).
- Task attributes are named XML attributes plus child elements for comments, custom
  attributes, people, categories and metadata (`TasklistSchemaDef.h:62-116`). A
  structural diff can therefore match tasks by ID and compare named values.
- The file is UTF-16 and normally written as **one line**: there is no whitespace between
  task elements. Line-based diffs are therefore not usable without canonicalisation.
  Example: `Core/ToDoList/Unicode_Debug/Resources/Examples/Workload.tdl` is a single line.
- Each task also stores derived/calculated values such as `CALC*`, `POSSTRING`,
  `SUBTASKDONE`, text/back colours and display strings (`TasklistSchemaDef.h:118-151`).
  These can change without a user-visible edit and must be excluded from change detection
  or recomputed.
- Repeated child elements (`PERSON`, `CATEGORY`, `TAG`, `DEPENDS`) must be compared as
  sets or ordered lists, not as raw text.
- zlib is already available: `Compression::Compress/Decompress`
  (`Core/3rdParty/Compression.cpp:8,20-46`). No new compression dependency is required.

### Measured costs

Using `Core/ToDoList/Unicode_Debug/Resources/Examples/biglist.tdl`:

| Measurement | Result |
|---|---|
| Raw tasklist | 1,719,182 bytes |
| gzip-compatible compression (Optimal) | 342,955 bytes |
| gzip ratio | ~20% (about 5x smaller) |
| 50 raw backups | ~86 MB |
| 50 gzip backups | ~17 MB |
| Change between a 16 Nov and a 20 Nov backup | ~53 KB of XML text changed (~3%) |
| Change between two saves one minute apart | 16 bytes |

The last two rows show why full snapshots are wasteful even when the file is large: a
single save usually touches a few values, while a full copy stores every task again.

## Options

| Option | Space | History/day view | Effort | Notes |
|---|---|---|---|---|
| A. Compress each backup | ~5x smaller | No | Low | Same full-snapshot semantics. Isolated change in `CTDCFileBackup` or a new wrapper. No cross-snapshot dedup. |
| B. XML-aware per-task deltas | Very small, often <1% per save | Yes | Medium | Base snapshot plus a chain of per-save deltas keyed by task ID. Needs canonicalisation, rebase and tombstones. |
| C. Content-addressed task store + snapshot manifests | Very small | Yes, random access | Medium-high | Store each distinct task version once by hash; each snapshot is a manifest of task ID to version. Easier browsing than a delta chain; needs compaction and metadata per snapshot. |
| D. Binary delta (xdelta/VCDIFF/bsdiff) | Very small | Weak | Medium | New dependency, no schema knowledge, restore chain, cannot explain which attribute changed. Good only for generic byte backup. |
| E. External VCS (git) | Excellent | Excellent | Rejected | User does not want tasklists under external source control. Also changes the file into a repository working copy. |

**Recommendation:** do A now, then B as the main feature. Keep the existing full backup
as a safety net; history is additive, not a replacement for a restorable snapshot.

## Design

### Storage layout

Store history beside the current backup folder, not inside the `.tdl`:

```text
<backup folder>/<tasklist name>/
  history.xml              manifest: bases, chains, retention (rebuildable cache)
  base-0001.tdl.gz         full compressed base snapshot
  journal-0001.tdlj        segmented append-only journal
  journal-0002.tdlj
```

The existing timestamped `.tdl` backups can remain unchanged. If the user enables
compact history, the old backups can be reduced to the configured minimum; history
provides the finer-grained versions. `.tdlj` is an internal history container, not a
tasklist format; the `.tdl` and its schema are untouched.

There are three physical layouts worth considering:

| Layout | Pros | Cons |
|---|---|---|
| One file per delta (`00000042.delta.gz`) | Simple create/rename atomicity; easy retention and corruption isolation; easiest first implementation | Many small files and directory entries; slow to list on network shares |
| **Segmented append-only journal (recommended)** | Few files; cheap append; one flush per save; segments align with retention | Needs record framing, partial-tail recovery and compaction |
| Single append-only journal | Simplest mental model | Grows forever; compaction rewrites the whole file; one corrupt region affects the whole history |

Recommended: **segmented journal**, one segment per day or per ~4 MB, with each record
compressed independently. If implementation speed matters more than file count, start
with one file per delta and migrate to segments later; the logical record format is the
same.

The history store must be a **sidecar**. It does not change the `.tdl` schema, so no
`TaskFile`/`TasklistSchemaDef` format bump is needed and the file remains readable by
upstream builds.

### Delta model

Key every task by `TASK.ID`. A delta record contains:

- snapshot metadata: local time, UTC `FILETIME`, application version, `FILEVERSION`,
  computer/user (reuse the source-control ID rules), and the base snapshot ID
- `ADD`: full task record for newly created tasks
- `DELETE`: task ID plus title, for reporting; a full record is optional
- `MODIFY`: task ID plus a list of `NAME / OLD / NEW` attribute values
- `MOVE`: task ID, new parent ID and ordinal, and the old values for display
- `COMMENTS`: old/new comment blocks, or a nested text delta if comments are large
- `CUSTOM`: custom-attribute IDs and old/new values
- `META`: file-level project name/path, custom attribute definitions, `NEXTUNIQUEID`,
  archive flag, encryption state

Replay is forward from the nearest base. Storing both old and new values makes both
forward restore and change display possible; deltas are small enough that the duplication
is acceptable. If space becomes critical, store only new values and derive the backward
direction by replaying from the previous base.

Use a versioned record (`HISTORY VERSION="1"`) and preserve unknown XML attributes
generically so a future schema can still read old history.

### Record framing and journal mechanics

A journal is a sequence of framed records. Each record is:

```text
+----------------------------+
| fixed header (little-endian)|
+----------------------------+
| zlib-compressed payload     |
+----------------------------+
| optional trailing CRC       |
+----------------------------+
```

Suggested header (48-64 bytes; exact layout is versioned by `headerVersion`):

| Field | Size | Purpose |
|---|---|---|
| `magic` | 4 | `"TDLJ"` |
| `headerVersion` | u16 | framing compatibility |
| `recordType` | u8 | delta, base, cutoff/rebase, commit |
| `flags` | u8 | compressed, encrypted, payload encoding |
| `headerSize` | u16 | allows future extension |
| `sequence` | u64 | monotonically increasing snapshot number |
| `baseSequence` | u64 | base this record belongs to |
| `prevSequence` | u64 | previous record in the chain; `0` at chain start |
| `timestampUtc` | u64 | Windows `FILETIME` |
| `payloadCompressedSize` | u32 | bytes following the header |
| `payloadUncompressedSize` | u32 | for allocation and sanity checks |
| `payloadCrc32` | u32 | integrity check, plus zlib's own Adler-32 |

The payload is UTF-8 XML, zlib-compressed with
`Compression::Compress` (`Core/3rdParty/Compression.cpp:20-46`). UTF-8 is internal to the
journal and keeps the delta smaller than UTF-16; the `.tdl` itself stays UTF-16. Use a
versioned root and an explicit encoding declaration so the payload can be parsed with
the existing `CXmlItem`/`CXmlFileEx` code.

Example delta payload:

```xml
<DELTA VERSION="1">
  <SNAPSHOT SEQ="42" BASE="1" PREV="41" UTC="2026-11-20T16:57:00Z"
            USER="PC\Robin" APP="1.0.0.0" FILEVER="47"/>
  <ADDS>
    <TASK ID="57" PARENTID="0" POS="2" TITLE="New task" ... />
  </ADDS>
  <DELETES>
    <TASK ID="13" TITLE="Old task"/>
  </DELETES>
  <MODIFIES>
    <TASK ID="6" TITLE="Activity 2.1">
      <ATTR NAME="TITLE" OLD="Activity 2.1" NEW="Activity 2.1 revised"/>
      <ATTR NAME="DUEDATE" OLD="46370.00000000" NEW="46372.00000000"/>
      <COMMENTS><OLD>...</OLD><NEW>...</NEW></COMMENTS>
      <CUSTOM ID="..." OLD="..." NEW="..."/>
      <MOVE OLD_PARENT="1" OLD_POS="2" NEW_PARENT="1" NEW_POS="3"/>
    </TASK>
  </MODIFIES>
  <META NEXTUNIQUEID="58" PROJECTNAME="..."/>
</DELTA>
```

The compression unit is one record, not the whole segment, so a record can be appended
without decompressing earlier data and a corrupt record does not poison the rest of the
segment. If a segment is closed for long-term storage it may optionally be recompressed
as a solid archive; that trades random access for a better ratio.

`history.xml` is a rebuildable cache, not the source of truth:

```xml
<HISTORY VERSION="1">
  <TASKLIST PATH="..." FILEID="..." />
  <BASE ID="1" FILE="base-0001.tdl.gz" SEQ="1" TIME="..." FILEVER="3"/>
  <CHAIN BASE="1" HEAD="42" RECORDS="41" BYTES="..."/>
  <RECORD SEQ="42" SEGMENT="journal-0001.tdlj" OFFSET="..." LENGTH="..."/>
</HISTORY>
```

Update the manifest with a temp file plus `MoveFileEx(MOVEFILE_REPLACE_EXISTING |
MOVEFILE_WRITE_THROUGH)`. If it is missing or stale, rebuild it by scanning segment
headers and following `prevSequence`.

Crash and concurrency rules:

- Write and flush the journal record, then update the manifest. If the process dies
  between the two, the record is recoverable by scanning and the manifest is rebuilt.
- Ignore a trailing partial record. Scan forward for the next `magic`; a bad CRC
  invalidates only that record and everything after it in the chain.
- Write history only after `tasks.Save` succeeds. If the process dies after the `.tdl`
  save but before the journal write, the history is one save behind; on the next save,
  compare `FILEVERSION` and/or a full-file hash against the last recorded snapshot and
  write a catch-up delta or start a new base.
- The built-in source-control lock protects the tasklist, not the history folder. Use a
  short-lived `history.lock` (`CreateFile` without sharing) for manifest/segment writes,
  or per-user segment files, so two users on a shared backup folder cannot corrupt the
  manifest. If the lock is unavailable, skip history for that save rather than block the
  save.
- Retention is prefix deletion. To drop old history, replay to the cutoff, write a new
  base at that state, then delete whole segments and older bases that no retained record
  references. Never delete a base that a retained delta still needs.

### Canonicalisation rules

Compare parsed XML values, never bytes or lines:

- Ignore attributes that are purely derived: `CALC*`, `POSSTRING`, display strings,
  `SUBTASKDONE`, and colour values that are recomputed from task attributes. Confirm
  which colour fields are explicitly user-set before excluding them.
- Keep `LASTMOD`/`LASTMODBY` as metadata on the task record, but do not count a
  timestamp-only change as a content change. Emit it separately so a history view can
  still show "touched".
- Normalise repeated elements to sets or ordered lists.
- Normalise task ordering to `(task ID, parent ID, ordinal)`; do not emit every sibling
  position when a parent moves.
- Compare child comments, custom attributes and metadata as structured values.
- Treat `FILEVERSION`, `NEXTUNIQUEID`, `FILENAME`, `LASTMOD` and app version as
  file-level metadata, not per-task content.

### Rebase and retention

- Start with one base snapshot when history is created.
- Rebase (write a new full base and stop depending on older deltas) every N saves, when
  the accumulated delta size exceeds a fraction of the base, or at a time boundary such
  as one base per day/week.
- Retain deltas for a time window (for example 30 or 90 days) and keep at least one base.
  This is deliberately different from the current "N copies per app version" rule.
- Never delete a base that a retained delta still needs. A simple manifest with
  `base`/`next` chains and a compaction pass is sufficient.
- Skip no-op saves. A save that produces an empty delta should update only a lightweight
  "last seen" marker, or be ignored entirely.

### Save hook

The clean hook is `CToDoCtrl::SaveTaskfile` (`Core/ToDoList/ToDoCtrl.cpp:4464-4484`):

- the old file still exists on disk;
- the new content is the in-memory `CTaskFile` passed in;
- the same method is used by normal saves, check-in and archive writes
  (`ToDoCtrl.cpp:4450`, `Core/ToDoList/TDCSourceControl.cpp:259,321,342`,
  `ToDoCtrl.cpp:5193`).

Record history only when `tasks.Save` succeeds, and only for the main tasklist path, not
for archives or exports. Pass a flag or check `tasks.IsArchive()` at the call site.
Because `SaveTaskfile` is static, a small history manager can be initialised from
preferences and called through a static/instance helper; alternatively make the history
call from the `Save` call site where `this` and preferences are already available. The
important ordering is: build new tasks -> compare with old file -> write the delta ->
write the new file. If the write fails, discard the delta.

`CToDoCtrl::Save` is the alternative hook if static state is undesirable, but then the
history record would miss check-in/archive paths unless they are handled explicitly.

### Day and task history

A manifest entry per snapshot is enough for both queries:

- **State at a time** = nearest base plus deltas up to that timestamp. Materialise in
  memory; never rewrite the real tasklist unless the user asks to restore.
- **Changes on a day** = select snapshots whose local date is in range, then aggregate
  old values from the first selected snapshot and new values from the last. Optionally
  show each save separately.
- **Task history** = filter all deltas for one task ID and show the old/new values in
  order.

The viewer can be a normal dialog or a generated tasklist. A read-only virtual tasklist
would reuse the Home/Plugin Manager pattern, but a simple list of changed tasks with
timestamp, task title and changed-attribute summary is enough for the first version.
Restore should be explicit and safe: restore to a copy or create a full backup first.

## Phases

| Phase | Scope | Risk |
|---|---|---|
| 1 | Compress new backups (`.gz` or zip) and optionally compress existing ones; no retention change | Low; backup path only |
| 2 | History store: manifest, base snapshots, per-save deltas, canonical diff, rebase and retention; no UI; verify by reconstructing snapshots in tests | Medium; new code, but sidecar only |
| 3 | Viewer: browse days/snapshots/tasks and show old/new values; optional restore-to-copy | Medium; UI and restore safety |

Phase 1 is independently useful. Phase 2 can land without any visible command and be
validated with tests. Phase 3 is the user-facing payoff.

## Files (expected)

- `Core/ToDoList/TDCFileBackup.h/.cpp` — compressed backup option, history entry point or wrapper.
- New `Core/ToDoList/TDCTaskHistory.h/.cpp` (or `TaskHistory.*`) — manifest, base/delta
  storage, canonical diff, replay, retention.
- `Core/ToDoList/ToDoCtrl.h/.cpp` — call the history recorder from `SaveTaskfile`/`Save`
  with the new `CTaskFile`.
- `Core/ToDoList/PreferencesFile2Page.*` — backup compression and history settings.
- `Core/ToDoList/PreferencesDlg.*`, `ToDoList.rc`, `resource.h` — optional history command,
  strings and a read-only viewer.
- `Core/ToDoListLib/ToDoListLib.vcxproj(.filters)` — new sources.
- `Core/TDLTest/` — diff/replay/retention tests.

No change to `TaskFile`, `TasklistSchemaDef`, `TDC_ATTRIBUTE` or `TDC_COLUMN` order.
No change to the source-control lock protocol.

## Tests and manual checks

Automated (`TDLTest`):

- no-op save produces no delta
- add, delete, modify, move, reorder and rename tasks produce the expected records
- comments and custom attributes round-trip through forward replay
- timestamp-only and derived-attribute changes are not reported as content changes
- every reconstructed snapshot matches the original `CTaskFile` semantically
- rebase and retention keep every reachable delta and never delete the last base
- a truncated/corrupt delta is ignored and the previous snapshot remains usable
- an encrypted tasklist either stores an encrypted history or is rejected with a clear
  status; never silently writes plaintext history
- performance on a large list (for example `biglist.tdl`) stays within a reasonable
  save-time budget

Manual:

- enable history, save several times with small edits, confirm the backup folder grows
  by kilobytes rather than megabytes
- view changes for a date and for a single task
- restore an older version to a copy and open it
- confirm existing backups, source control check-in/out and archive still work
- confirm no history is written for exports, archives or failed saves
- confirm a shared backup folder with two users does not corrupt the manifest

## Risks and limitations

- **Encryption:** if a tasklist has a password, the on-disk XML is encrypted
  (`EncryptDecrypt.dll` via `CXmlFileEx`), and the current backup copies the encrypted
  bytes. A semantic diff needs the password at save time. Store history encrypted with
  the same password, or disable history for encrypted tasklists. Never write plaintext
  task content beside an encrypted tasklist.
- **Shared folders:** backups can be relative to the tasklist and therefore shared. The
  built-in source control lock prevents concurrent saves of the tasklist, but history
  writes still need an atomic append/lock or per-user files to avoid a corrupt manifest.
- **Derived attributes:** excluding the wrong fields either hides real edits or creates
  noise. The canonicalisation list needs a careful audit against
  `ToDoCtrlDataUtils` and the task writer.
- **Schema evolution:** history records are a second persisted format. Version them and
  preserve unknown attributes; otherwise a future `.tdl` change can make old history
  unreadable.
- **Large comments:** a comment edit is one large value. Store an optional nested text
  delta only if measurements show it matters.
- **Auto-save:** frequent auto-save can create many tiny deltas. Coalesce same-minute
  saves or skip identical content.
- **Privacy:** history retains deleted tasks, comments and metadata. It needs the same
  access expectations as the tasklist and the existing backups.
- **Not a merge tool:** history is observational. It does not resolve concurrent edits
  and does not replace the source-control lock or existing conflict prompt.
- **Do not remove the full snapshot path:** history replay depends on a valid base. Keep
  the existing full-copy backup as the recovery path and as the first base.
