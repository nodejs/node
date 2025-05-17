#!/usr/bin/env pwsh

$NODE_EXE="$PSScriptRoot/node.exe"
if (-not (Test-Path $NODE_EXE)) {
  $NODE_EXE="$PSScriptRoot/node"
}
if (-not (Test-Path $NODE_EXE)) {
  $NODE_EXE="node"
}

$NPM_PREFIX_JS="$PSScriptRoot/node_modules/npm/bin/npm-prefix.js"
$NPX_CLI_JS="$PSScriptRoot/node_modules/npm/bin/npx-cli.js"
$NPM_PREFIX=(& $NODE_EXE $NPM_PREFIX_JS)

if ($LASTEXITCODE -ne 0) {
  Write-Host "Could not determine Node.js install directory"
  exit 1
}

$NPM_PREFIX_NPX_CLI_JS="$NPM_PREFIX/node_modules/npm/bin/npx-cli.js"
if (Test-Path $NPM_PREFIX_NPX_CLI_JS) {
  $NPX_CLI_JS=$NPM_PREFIX_NPX_CLI_JS
}

if ($MyInvocation.Line) { # used "-Command" argument
  if ($MyInvocation.Statement) {
    $NPX_ARGS = $MyInvocation.Statement.Substring($MyInvocation.InvocationName.Length).Trim()
  } else {
    $NPX_OG_COMMAND = (
      [System.Management.Automation.InvocationInfo].GetProperty('ScriptPosition', [System.Reflection.BindingFlags] 'Instance, NonPublic')
    ).GetValue($MyInvocation).Text
    $NPX_ARGS = $NPX_OG_COMMAND.Substring($MyInvocation.InvocationName.Length).Trim()
  }

  $NODE_EXE = $NODE_EXE.Replace("``", "````")
  $NPX_CLI_JS = $NPX_CLI_JS.Replace("``", "````")

  # Support pipeline input
  if ($MyInvocation.ExpectingInput) {
    $input = (@($input) -join "`n").Replace("``", "````")

    Invoke-Expression "Write-Output `"$input`" | & `"$NODE_EXE`" `"$NPX_CLI_JS`" $NPX_ARGS"
  } else {
    Invoke-Expression "& `"$NODE_EXE`" `"$NPX_CLI_JS`" $NPX_ARGS"
  }
} else { # used "-File" argument
  # Support pipeline input
  if ($MyInvocation.ExpectingInput) {
    $input | & $NODE_EXE $NPX_CLI_JS $args
  } else {
    & $NODE_EXE $NPX_CLI_JS $args
  }
}

exit $LASTEXITCODE
