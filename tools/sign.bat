@echo off

set timeservers=(http://timestamp.globalsign.com/scripts/timestamp.dll http://timestamp.comodoca.com/authenticode http://timestamp.verisign.com/scripts/timestamp.dll http://tsa.starfieldtech.com)

for %%s in %timeservers% do (
    signtool sign /a /d "Node.js" /du "https://nodejs.org" /t %%s %1
    if not ERRORLEVEL 1 (
        echo Successfully signed %1 using timeserver %%s
        exit /b 0
    )
    echo Signing %1 failed using %%s
)

echo Could not sign %1 using any available timeserver
exit /b 1
