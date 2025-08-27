@echo OFF

echo.
echo ===============================================================================
echo  CLEANING PROJECT...
echo ===============================================================================
echo.

REM Change to the script's directory to ensure we are in the project root
cd /D "%~dp0"

REM Verify we are in the correct directory by checking for the .uproject file
IF NOT EXIST "VrPiano554.uproject" (
    echo ERROR: Could not find VrPiano554.uproject file.
    echo Please run this script from the root directory of your Unreal project.
    pause
    exit /b 1
)

echo Deleting generated folders...
echo.

REM List of folders to delete
set FOLDERS_TO_DELETE=Binaries Intermediate DerivedDataCache Saved .vs

REM Loop through and delete each folder
for %%f in (%FOLDERS_TO_DELETE%) do (
    if exist %%f (
        echo Deleting %%f folder...
        rmdir /s /q %%f
    ) else (
        echo %%f folder not found, skipping.
    )
)

echo.
echo ===============================================================================
echo  CLEANUP COMPLETE
echo ===============================================================================
echo.
echo You can now right-click on your .uproject file and select 'Generate Visual Studio project files'.
echo After that, open the .sln file and build the project from Visual Studio.
echo.
pause
