:: Created by npm, please don't edit manually.
@ECHO OFF

SETLOCAL

SET "NODE_EXE=%~dp0\node.exe"
IF NOT EXIST "%NODE_EXE%" (
  SET "NODE_EXE=node"
)

SET "NPX_CLI_JS=%~dp0\node_modules\npm\bin\npx-cli.js"
FOR /F "delims=" %%F IN ('CALL "%NODE_EXE%" "%NPX_CLI_JS%" prefix -g') DO (
  SET "NPM_PREFIX_NPX_CLI_JS=%%F\node_modules\npm\bin\npx-cli.js"
)
IF EXIST "%NPM_PREFIX_NPX_CLI_JS%" (
  SET "NPX_CLI_JS=%NPM_PREFIX_NPX_CLI_JS%"
)

"%NODE_EXE%" "%NPX_CLI_JS%" %*
