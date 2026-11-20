# Plugins in the release build

Inventory of the plugin DLLs shipped in `Core/ToDoList/unicode_release` as of the
2026-11-19 13:53 build.

- **96 DLLs** are present in the folder.
- **31** export a plugin factory and are listed below.
- **65** are support/runtime DLLs (managed `*Core.dll` assemblies, third-party
  libraries, MFC/CRT/runtime DLLs) and are not plugins.

The data was collected without loading the DLLs: the PE export directory was read
for the factory names and `VERSIONINFO` for the version fields. The actual
importer/exporter behaviour was cross-checked in the project sources, because
export-only bridges also export a stub `CreateImportInterface` that returns
`NULL`.

## Summary

| Type | Total | Core | External |
|---|---:|---:|---:|
| UI Extension | 13 | 5 | 8 |
| Content | 4 | 1 | 3 |
| Import/Export | 6 | 5 | 1 |
| Export | 6 | 0 | 6 |
| Storage | 2 | 1 | 1 |
| **Total** | **31** | **12** | **19** |

## Plugin list

Sorted by type, then by file name. `Core` means the DLL is built from the
`Core/` solution tree; `No` means it comes from the `Plugins/` tree.

| File name | Version (file / product) | Description | Type | Core? | Direct dependencies | Source project |
|---|---|---|---|---|---|---|
| BurndownExt.dll | 1.0.0.0 / 1.0.0.1 | Graphs (burn-down/status charts) task view | UI Extension | Yes | MFC 14 (`mfc140u.dll`) | `Core/BurndownExt/BurndownExt.vcxproj` |
| CalendarExt.dll | 1.0.0.0 / n/a | Calendar task view | UI Extension | Yes | MFC 14 (`mfc140u.dll`) | `Core/CalendarExt/CalendarExt.vcxproj` |
| EisenhowerUIExtensionBridge.dll | 1.0.0.0 / 1.0.0.1 | Decision Matrix (Eisenhower matrix) task view | UI Extension | No | .NET 4.x (`mscoree.dll`), CommandHandling.dll, EisenhowerUIExtensionControls.dll, EisenhowerUIExtensionCore.dll, EisenhowerUIExtensionData.dll, ImageHelper.dll, Itenso.Solutions.Community.Rtf2Html.dll, Microsoft.Office.Interop.Outlook.dll\*, Microsoft.VisualStudio.OLE.Interop.dll, PluginHelpers.dll, ScrollHelper.dll, ToolStripToolTip.dll, UIComponents.dll | `Plugins/UIExtension/EisenhowerUIExtension/EisenhowerUIExtensionBridge/EisenhowerUIExtensionBridge.vcxproj` |
| EvidenceBoardUIExtensionBridge.dll | 1.0.0.0 / 1.0.0.1 | Evidence Board task view | UI Extension | No | .NET 4.x (`mscoree.dll`), EvidenceBoardUIExtensionCore.dll, ImageHelper.dll, PluginHelpers.dll, RadialTree.dll, ScrollHelper.dll, ToolStripToolTip.dll, TreeViewHelper.dll, UIComponents.dll | `Plugins/UIExtension/EvidenceBoardUIExtension/EvidenceBoardUIExtensionBridge/EvidenceBoardUIExtensionBridge.vcxproj` |
| GanttChartExt.dll | 1.0.0.0 / 1.0.0.1 | Gantt Chart task view | UI Extension | Yes | MFC 14 (`mfc140u.dll`) | `Core/GanttChartExt/GanttChartExt.vcxproj` |
| KanbanBoard.dll | 1.0.0.0 / 1.0.0.1 | Kanban board task view | UI Extension | Yes | MFC 14 (`mfc140u.dll`) | `Core/KanbanBoard/KanbanBoard.vcxproj` |
| LoggedTimeUIExtensionBridge.dll | 1.0.0.0 / 1.0.0.1 | Logged Time task view | UI Extension | No | .NET 4.x (`mscoree.dll`), Calendar.DayView.dll, LinkLabelEx.dll, LoggedTimeUIExtensionCore.dll, PluginHelpers.dll, ToolStripToolTip.dll, UIComponents.dll, WeekPlannerUIExtensionCore.dll | `Plugins/UIExtension/LoggedTimeUIExtension/LoggedTimeUIExtensionBridge/LoggedTimeUIExtensionBridge.vcxproj` |
| MindMapUIExtensionBridge.dll | 1.0.0.0 / 1.0.0.1 | Mind Map task view | UI Extension | No | .NET 4.x (`mscoree.dll`), ImageHelper.dll, MindMapUIExtensionCore.dll, PluginHelpers.dll, ScrollHelper.dll, TreeViewHelper.dll | `Plugins/UIExtension/MindMapUIExtension/MindMapUIExtensionBridge/MindMapUIExtensionBridge.vcxproj` |
| SampleUIExtensionBridge.dll | 1.0.0.1 / 1.0.0.1 | Sample UI extension (template/demo) | UI Extension | No | .NET 4.x (`mscoree.dll`), PluginHelpers.dll, SampleUIExtensionCore.dll | `Plugins/UIExtension/SampleUIExtension/SampleUIExtensionBridge/SampleUIExtensionBridge.vcxproj` |
| TaskDatesUIExtensionBridge.dll | 1.0.0.0 / 1.0.0.1 | Task Dates task view | UI Extension | No | .NET 4.x (`mscoree.dll`), CommandHandling.dll, ImageHelper.dll, Itenso.Solutions.Community.Rtf2Html.dll, Microsoft.Office.Interop.Outlook.dll\*, Microsoft.VisualStudio.OLE.Interop.dll, PluginHelpers.dll, ScrollHelper.dll, TaskDatesUIExtensionCore.dll, ToolStripToolTip.dll | `Plugins/UIExtension/TaskDatesUIExtension/TaskDatesUIExtensionBridge/TaskDatesUIExtensionBridge.vcxproj` |
| WeekPlannerUIExtensionBridge.dll | 1.0.0.0 / 1.0.0.1 | Week Planner task view | UI Extension | No | .NET 4.x (`mscoree.dll`), Calendar.DayView.dll, LinkLabelEx.dll, PluginHelpers.dll, ToolStripToolTip.dll, UIComponents.dll, WeekPlannerUIExtensionCore.dll | `Plugins/UIExtension/WeekPlannerUIExtension/WeekPlannerUIExtensionBridge/WeekPlannerUIExtensionBridge.vcxproj` |
| WordCloudUIExtensionBridge.dll | 1.0.0.0 / 1.0.0.1 | Word Cloud task view | UI Extension | No | .NET 4.x (`mscoree.dll`), Gma.CodeCloud.Controls.dll, PluginHelpers.dll, ToolStripToolTip.dll, UIComponents.dll, WordCloudUIExtensionCore.dll | `Plugins/UIExtension/WordCloudUIExtension/WordCloudUIExtensionBridge/WordCloudUIExtensionBridge.vcxproj` |
| WorkloadExt.dll | 1.0.0.0 / 1.0.0.1 | Workload chart task view | UI Extension | Yes | MFC 14 (`mfc140u.dll`) | `Core/WorkloadExt/WorkloadExt.vcxproj` |
| HTMLContentControlBridge.dll | 1.0.0.0 / 1.0.0.1 | HTML comments/content editor | Content | No | .NET 4.x (`mscoree.dll`), CommandHandling.dll, CustomComboBox.dll, HTMLContentControlCore.dll, ImageHelper.dll, Microsoft.Office.Interop.Outlook.dll\*, Microsoft.VisualStudio.OLE.Interop.dll, MSDN.HtmlEditorControl.dll, PluginHelpers.dll, unvell.ReoGrid.dll, WebBrowserEx.dll | `Plugins/ContentControl/HTMLContentControl/HTMLContentControlBridge/HTMLContentControlBridge.vcxproj` |
| MDContentControlBridge.dll | 1.0.0.0 / 1.0.0.1 | Markdown comments/content editor | Content | No | .NET 4.x (`mscoree.dll`), CommandHandling.dll, ImageHelper.dll, Markdig.dll, MDContentControlCore.dll, MSDN.HtmlEditorControl.dll, PluginHelpers.dll, ReverseMarkdown.dll, WebBrowserEx.dll | `Plugins/ContentControl/MDContentControl/MDContentControlBridge/MDContentControlBridge.vcxproj` |
| RTFContentCtrl.dll | 1.0.0.0 / 1.0.0.0 | Rich Text comments/content editor | Content | Yes | MFC 14 (`mfc140u.dll`), Rtf2HtmlBridge.dll (loaded on demand) | `Core/RTFContentCtrl/RTFContentCtrl.vcxproj` |
| SpreadsheetContentControlBridge.dll | 1.0.0.0 / 1.0.0.1 | Spreadsheet comments/content editor | Content | No | .NET 4.x (`mscoree.dll`), CommandHandling.dll, PluginHelpers.dll, SpreadsheetContentControlCore.dll, unvell.ReoGrid.dll, unvell.ReoGridEditorControl.dll | `Plugins/ContentControl/SpreadsheetContentControl/SpreadsheetContentControlBridge/SpreadsheetContentControlBridge.vcxproj` |
| FMindImportExport.dll | 1.0.0.0 / 1.0.0.1 | FreeMind mind-map import/export | Import/Export | Yes | MFC 14 (`mfc140u.dll`) | `Core/FMindImportExport/FMindImportExport.vcxproj` |
| GPExport.dll | 1.0.0.0 / 1.0.0.1 | GanttProject import/export | Import/Export | Yes | MFC 14 (`mfc140u.dll`) | `Core/GPExport/GPExport.vcxproj` |
| iCalImportExport.dll | 1.0.0.0 / 1.0.0.1 | iCalendar import/export | Import/Export | Yes | MFC 14 (`mfc140u.dll`) | `Core/iCalImportExport/iCalImportExport.vcxproj` |
| MLOImport.dll | 1.0.0.0 / 1.0.0.1 | My Life Organized import/export | Import/Export | Yes | MFC 14 (`mfc140u.dll`) | `Core/MLOImport/MLOImport.vcxproj` |
| PlainTextImport.dll | 1.0.0.0 / 1.0.0.1 | Outline (plain text) import/export | Import/Export | Yes | MFC 14 (`mfc140u.dll`) | `Core/PlainTextImport/PlainTextImport.vcxproj` |
| TodoTxtImpExpBridge.dll | 1.0.0.0 / 1.0.0.1 | todo.txt import/export | Import/Export | No | .NET 4.x (`mscoree.dll`), PluginHelpers.dll, TodoLib.dll, TodoTxtImpExpCore.dll | `Plugins/ImportExport/TodoTxtImpExp/TodoTxtImpExpBridge/TodoTxtImpExpBridge.vcxproj` |
| HTMLReportExporterBridge.dll | 1.0.0.0 / 1.0.0.1 | Report Builder HTML export | Export | No | .NET 4.x (`mscoree.dll`), CommandHandling.dll, HtmlAgilityPack.dll, HTMLReportExporterCore.dll, MSDN.HtmlEditorControl.dll, PluginHelpers.dll, ToolStripToolTip.dll, UIComponents.dll, WebBrowserEx.dll | `Plugins/ImportExport/HTMLReportExporter/HTMLReportExporterBridge/HTMLReportExporterBridge.vcxproj` |
| JSONExporterBridge.dll | 1.0.0.0 / 1.0.0.1 | JSON export | Export | No | .NET 4.x (`mscoree.dll`), JSONExporterCore.dll, Newtonsoft.Json.dll, PluginHelpers.dll | `Plugins/ImportExport/JSONExporter/JSONExporterBridge/JSONExporterBridge.vcxproj` |
| MarkdeepExporterBridge.dll | 1.0.0.1 / 1.0.0.1 | Markdeep export | Export | No | .NET 4.x (`mscoree.dll`), MarkdeepExporterCore.dll, MarkdownLog.dll, PluginHelpers.dll | `Plugins/ImportExport/MarkdeepExporter/MarkdeepExporterBridge/MarkdeepExporterBridge.vcxproj` |
| OutlookExporterBridge.dll | 1.0.0.1 / 1.0.0.1 | Microsoft Outlook export | Export | No | .NET 4.x (`mscoree.dll`), OutlookExporterCore.dll, PluginHelpers.dll | `Plugins/ImportExport/OutlookExporter/OutlookExporterBridge/OutlookExporterBridge.vcxproj` |
| PDFExporterBridge.dll | 1.0.0.0 / 1.0.0.1 | PDF Tree View export | Export | No | .NET 4.x (`mscoree.dll`), HtmlAgilityPack.dll, ImageHelper.dll, iTextSharp.dll, MSDN.HtmlEditorControl.dll, PDFExporterCore.dll, PluginHelpers.dll | `Plugins/ImportExport/PDFExporter/PDFExporterBridge/PDFExporterBridge.vcxproj` |
| SampleImpExpBridge.dll | 1.0.0.1 / 1.0.0.1 | Sample exporter (template/demo) | Export | No | .NET 4.x (`mscoree.dll`), PluginHelpers.dll, SampleImpExpCore.dll | `Plugins/ImportExport/SampleImpExp/SampleImpExpBridge/SampleImpExpBridge.vcxproj` |
| FtpStorage.dll | 1.0.0.0 / 1.0.0.1 | FTP server tasklist storage | Storage | Yes | MFC 14 (`mfc140u.dll`) | `Core/FtpStorage/FtpStorage.vcxproj` |
| MySQLStorageBridge.dll | 1.0.0.0 / n/a | MySQL database tasklist storage | Storage | No | .NET 4.x (`mscoree.dll`), MySql.Data.dll, MySqlStorageCore.dll, PluginHelpers.dll, UIComponents.dll | `Plugins/TasklistStorage/MySqlStorage/MySqlStorageBridge/MySqlStorageBridge.vcxproj` |

\* `Microsoft.Office.Interop.Outlook.dll` is referenced by three plugins but is
**not present** in `Core/ToDoList/unicode_release`. It is present in
`Plugins/release`. The affected plugins may still load and run; only the code
paths that touch the interop assembly fail.

## Notes

- **Version** is `VERSIONINFO` `FileVersion` / `ProductVersion`. It is the
  embedded resource version, not a per-plugin release number. `FileVersion` is
  `1.0.0.0` for most DLLs and `1.0.0.1` for the four
  `SampleUIExtensionBridge`, `SampleImpExpBridge`, `OutlookExporterBridge` and
  `MarkdeepExporterBridge`; `ProductVersion` is `1.0.0.1` where present and
  blank for `CalendarExt` and `MySQLStorageBridge`.
- **Type** is the interface actually implemented. All seven bridge plugins under
  `Plugins/ImportExport` and the five core import/export plugins export both
  `CreateImportInterface` and `CreateExportInterface`, but the six export-only
  bridges return `NULL` from the importer factory; those are listed as `Export`.
- **Dependencies** are the direct shipped DLLs used by the plugin or its managed
  `*Core` assembly, plus the required runtime (MFC 14 or .NET Framework 4.x).
  System DLLs are omitted, and transitive dependencies are not expanded. For
  example, a managed plugin's `*Core.dll` may in turn load additional libraries.
- **Core** classifies the source tree (`Core/` vs `Plugins/`), not whether the
  DLL is shipped with the application. All 31 DLLs are part of the release
  folder.
- The 65 non-plugin DLLs were still passed to `LoadLibrary` by the current
  scanner, but they export no recognised factory and are unloaded again.
