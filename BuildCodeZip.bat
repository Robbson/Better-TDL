ECHO OFF

REM - WE USE FULL PATHS THROUGHOUT - NO 'CD' WITH RELATIVE PATHS

PUSHD "%~dp0"
ECHO Zipping Code
ECHO ============
ECHO:

SET REPO=%CD%
CALL %REPO%\FindTools.bat
SET PATHPZIP=%PROJECTZIP%

ECHO REPO     = %REPO%
ECHO PATHPZIP = %PATHPZIP%
ECHO:

IF NOT DEFINED PATHPZIP (
ECHO [41m Unable to locate ProjectZip.exe!![0m
PAUSE
EXIT
)

MKDIR %TDL_PREV%\%TDL_PREV_VERSION% 2> NUL

REM Options
REM ZP - Zip Path
REM ML - Make Lowercase
REM NM - No Message

ECHO Zipping %REPO%\Core\ToDoList_Core.sln
%PATHPZIP% %REPO%\Core\ToDoList_Core.sln /ZP %TDL_PREV%\%TDL_PREV_VERSION%\ToDoList_src_Core.%TDL_PREV_VERSION%._.zip /ML /NM

ECHO Zipping %REPO%\Plugins\ToDoList_Plugins.sln
%PATHPZIP% %REPO%\Plugins\ToDoList_Plugins.sln /ZP %TDL_PREV%\%TDL_PREV_VERSION%\ToDoList_src_Plugins.%TDL_PREV_VERSION%._.zip /ML /NM

ECHO:
ECHO [42m Zipping Code SUCCEEDED[0m
ECHO:

POPD
