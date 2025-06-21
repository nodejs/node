@echo off

@REM From June 2025, we started using Azure Trusted Signing for code signing.
@REM Release CI machines are configured to have it in the PATH so this can be used safely.

where signtool >nul 2>&1
if errorlevel 1 (
    echo signtool not found in PATH.
    exit /b 1
)

if "%AZURE_SIGN_DLIB_PATH%"=="" (
    echo AZURE_SIGN_DLIB_PATH is not set.
    exit /b 1
)

if "%AZURE_SIGN_METADATA_PATH%"=="" (
    echo AZURE_SIGN_METADATA_PATH is not set.
    exit /b 1
)


signtool sign /tr "http://timestamp.acs.microsoft.com" /td sha256 /fd sha256 /v /dlib %AZURE_SIGN_DLIB_PATH% /dmdf %AZURE_SIGN_METADATA_PATH% %1
if not ERRORLEVEL 1 (
    echo Successfully signed %1 using signtool
    exit /b 0
)
echo Could not sign %1 using signtool
exit /b 1