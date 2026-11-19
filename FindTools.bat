ECHO OFF

REM - Shared tool discovery for the root build scripts
REM - CALL this from a script after setting REPO
REM - No absolute paths are assumed: each tool is taken from the environment if
REM - already set, then from PATH, then from well-known install locations.
REM - Located tools are written in the quoted form the scripts expect.

IF NOT DEFINED MSBUILD CALL :FindOnPath MSBuild.exe MSBUILD
IF NOT DEFINED MSBUILD CALL :FindVSWhereMSBuild
IF NOT DEFINED MSBUILD CALL :FindCommonMSBuild

IF NOT DEFINED MSDEV CALL :FindOnPath msdev.exe MSDEV
IF NOT DEFINED MSDEV CALL :FindCommonMSDev

IF NOT DEFINED SEVENZIP CALL :FindOnPath 7z.exe SEVENZIP
IF NOT DEFINED SEVENZIP CALL :FindCommon7Zip

IF NOT DEFINED RCEDIT CALL :FindOnPath rcedit-x64.exe RCEDIT
IF NOT DEFINED RCEDIT CALL :FindOnPath rcedit.exe RCEDIT
IF NOT DEFINED RCEDIT CALL :FindInRepo rcedit-x64.exe RCEDIT
IF NOT DEFINED RCEDIT CALL :FindInRepo rcedit.exe RCEDIT
IF NOT DEFINED RCEDIT IF EXIST "D:\tools\rcedit-x64.exe" SET "RCEDIT="D:\tools\rcedit-x64.exe""

IF NOT DEFINED PROJECTZIP CALL :FindOnPath ProjectZip.exe PROJECTZIP
IF NOT DEFINED PROJECTZIP CALL :FindInRepo ProjectZip.exe PROJECTZIP
IF NOT DEFINED PROJECTZIP IF EXIST "D:\Tools\ProjectZip.exe" SET "PROJECTZIP="D:\Tools\ProjectZip.exe""

REM - Sibling repositories and output folders
REM - Defaults assume a common parent folder; override via the environment
IF NOT DEFINED TDL_RESOURCES SET "TDL_RESOURCES=%REPO%\..\ToDoList_Resources"
IF NOT DEFINED TDL_TESTREPO SET "TDL_TESTREPO=%REPO%\..\ToDoList_Test"
IF NOT DEFINED TDL_SYMBOLS SET "TDL_SYMBOLS=%REPO%\..\ToDoList_Symbols"
IF NOT DEFINED TDL_DOWNLOADS SET "TDL_DOWNLOADS=%REPO%\..\ToDoList_Downloads"
IF NOT DEFINED TDL_PREV SET "TDL_PREV=%REPO%\..\ToDoList_Prev"
IF NOT DEFINED TDL_PREV_VERSION SET "TDL_PREV_VERSION=9.3"

REM - .NET Framework targeting: plugins target v4.0, which modern VS does not
REM - install. Only override when the v4.0 reference assemblies are absent.
SET "REFASM=%ProgramFiles(x86)%\Reference Assemblies\Microsoft\Framework\.NETFramework"
IF NOT EXIST "%REFASM%" SET "REFASM=%ProgramFiles%\Reference Assemblies\Microsoft\Framework\.NETFramework"
IF NOT DEFINED TDL_DOTNET_TFM IF NOT EXIST "%REFASM%\v4.0\mscorlib.dll" CALL :FindDotNetTfm

GOTO :EOF

REM ---------------------------------------------------------------------------

:FindOnPath
FOR /F "delims=" %%I IN ('where %~1 2^>NUL') DO IF NOT DEFINED %~2 SET "%~2="%%I""
GOTO :EOF

:FindInRepo
IF DEFINED REPO IF EXIST "%REPO%\tools\%~1" SET "%~2="%REPO%\tools\%~1""
GOTO :EOF

:FindVSWhereMSBuild
SET "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
IF NOT EXIST "%VSWHERE%" SET "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
IF NOT EXIST "%VSWHERE%" GOTO :EOF
FOR /F "usebackq delims=" %%I IN (`"%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe 2^>NUL`) DO IF NOT DEFINED MSBUILD SET "MSBUILD="%%I""
GOTO :EOF

:FindCommonMSBuild
FOR %%D IN (
"%ProgramFiles%\Microsoft Visual Studio\2022\Community"
"%ProgramFiles%\Microsoft Visual Studio\2022\Professional"
"%ProgramFiles%\Microsoft Visual Studio\2022\Enterprise"
"%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools"
"%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community"
"%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Professional"
"%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Enterprise"
"%ProgramFiles(x86)%\Microsoft Visual Studio\2019\BuildTools"
"%ProgramFiles(x86)%\MSBuild\14.0\Bin"
) DO (
IF NOT DEFINED MSBUILD IF EXIST "%%~D\MSBuild\Current\Bin\MSBuild.exe" SET "MSBUILD="%%~D\MSBuild\Current\Bin\MSBuild.exe""
IF NOT DEFINED MSBUILD IF EXIST "%%~D\MSBUILD\14.0\Bin\MSBuild.exe" SET "MSBUILD="%%~D\MSBUILD\14.0\Bin\MSBuild.exe""
IF NOT DEFINED MSBUILD IF EXIST "%%~D\MSBuild.exe" SET "MSBUILD="%%~D\MSBuild.exe""
)
GOTO :EOF

:FindCommonMSDev
FOR %%D IN (
"%ProgramFiles(x86)%\Microsoft Visual Studio\Common\MSDev98\Bin"
"%ProgramFiles%\Microsoft Visual Studio\Common\MSDev98\Bin"
) DO IF NOT DEFINED MSDEV IF EXIST "%%~D\msdev.exe" SET "MSDEV="%%~D\msdev.exe""
GOTO :EOF

:FindCommon7Zip
FOR %%D IN (
"%ProgramFiles%\7-Zip"
"%ProgramFiles(x86)%\7-Zip"
) DO IF NOT DEFINED SEVENZIP IF EXIST "%%~D\7z.exe" SET "SEVENZIP="%%~D\7z.exe""
GOTO :EOF

:FindDotNetTfm
FOR %%V IN (v4.8 v4.7.2 v4.6.2 v4.6.1 v4.6 v4.5.2 v4.5.1 v4.5) DO IF NOT DEFINED TDL_DOTNET_TFM IF EXIST "%REFASM%\%%V\mscorlib.dll" SET "TDL_DOTNET_TFM=%%V"
GOTO :EOF
