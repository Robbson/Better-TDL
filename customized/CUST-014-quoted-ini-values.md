# CUST-014: Quote-safe values in ToDoList.ini

Status: **Done**

## Goal

Stop the debug assertion that fires when ToDoList.ini is saved while the Quick Find
history contains a term with quotation marks (for example `"Jardy Glaube"`), and make
such terms survive a save/load round trip unchanged.

## Upstream starting point

Quick Find history is written on shutdown from `CToDoListWnd::SaveSettings()`
(`ToDoListWnd.cpp:2687-2693`) via `CPreferences::WriteProfileArray()` into the
`[QuickFind]` section.

The ini backend serialises values in `INIENTRY::Format()`
(`Core/Interfaces/Preferences.cpp:35`) and had:

```cpp
ASSERT(sValue.Find('\"') == -1);
```

so any value containing `"` tripped the assertion. In release builds the raw value was
written into the quoted entry (`Item0=""Jardy Glaube""`), and `INIENTRY::Parse()`
removed *all* quote characters when loading, so the quotes were silently lost.

`INIENTRY::SafeQuote()`/`UnSafeQuote()` already existed (using the `{QUOTES}`
placeholder) but were dead code.

## What changed

Files: `Core/Interfaces/Preferences.cpp`, `Core/TDLTest/PreferencesTest.h`,
`Core/TDLTest/PreferencesTest.cpp`.

1. `INIENTRY::Format()` escapes embedded quotes with `SafeQuote()` before composing the
   line. The assertion now checks the escaped value, so it still guards the invariant
   that no raw quote reaches the file.
2. `INIENTRY::Parse()` strips only the optional surrounding quotes and then calls
   `UnSafeQuote()`, restoring embedded quotes.
3. Because the in-memory entry keeps the raw value, `GetProfileString()` and
   `SetEntryValue()` equality checks are unaffected, and the registry backend is
   untouched.
4. Added `CPreferencesTest::QuotedStrings` which writes values with leading/trailing,
   embedded and lone quotes (both as single entries and via `WriteProfileArray`), saves,
   reloads the ini from disk and verifies the round trip.

## Risks

- The `{QUOTES}` placeholder is a sentinel: a literal value containing the text
  `{QUOTES}` would be converted to `"` on load. This limitation pre-existed in the
  unused helpers and is considered acceptable.
- Files written by older release builds have already lost embedded quotes in that
  section; nothing can restore them, but new saves preserve the terms.
- Only values are escaped. Section and entry names are unaffected.

## Verification

- `Unicode Debug` solution build succeeds.
- `TDLTest`: `CPreferencesTest::QuotedStrings` passes 5/5. The pre-existing 24
  `CTDCTaskMatcherTest::IndividualRules` text-array failures are unchanged and unrelated.
- Manual check: entering `"Jardy Glaube"` in Quick Find, closing ToDoList and reopening
  it no longer shows an assertion and the term is restored verbatim from
  `ToDoList.ini`.
