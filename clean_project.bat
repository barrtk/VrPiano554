@echo off
echo Cleaning Unreal Engine project...

echo Checking for Binaries folder...
IF EXIST "C:\Users\Bartek\Documents\Unreal Projects\VrPiano554\Binaries" (
    echo Binaries folder found. Attempting to delete...
    rmdir /s /q "C:\Users\Bartek\Documents\Unreal Projects\VrPiano554\Binaries"
    IF ERRORLEVEL 1 (
        echo ERROR: Failed to delete Binaries folder. It might be in use or permissions are insufficient.
    ) ELSE (
        echo Binaries folder deleted successfully.
    )
) ELSE (
    echo Binaries folder not found. Skipping deletion.
)

echo Deleting .vs folder...
rmdir /s /q "C:\Users\Bartek\Documents\Unreal Projects\VrPiano554\.vs"

echo Deleting Intermediate folder...
rmdir /s /q "C:\Users\Bartek\Documents\Unreal Projects\VrPiano554\Intermediate"

echo Deleting Saved folder...
rmdir /s /q "C:\Users\Bartek\Documents\Unreal Projects\VrPiano554\Saved"

echo Deleting VrPiano554.sln file...
del /f /q "C:\Users\Bartek\Documents\Unreal Projects\VrPiano554\VrPiano554.sln"

echo Deleting .vsconfig file...
del /f /q "C:\Users\Bartek\Documents\Unreal Projects\VrPiano554\.vsconfig"

echo Cleaning complete.
pause