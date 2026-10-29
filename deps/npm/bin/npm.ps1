#!/usr/bin/env pwsh

Set-StrictMode -Version 'Latest'

$NODE_EXE="$PSScriptRoot/node.exe"
if (-not (Test-Path $NODE_EXE)) {
  $NODE_EXE="$PSScriptRoot/node"
}
if (-not (Test-Path $NODE_EXE)) {
  $NODE_EXE="node"
}

$NPM_PREFIX_JS="$PSScriptRoot/node_modules/npm/bin/npm-prefix.js"
$NPM_CLI_JS="$PSScriptRoot/node_modules/npm/bin/npm-cli.js"
$NPM_PREFIX=(& $NODE_EXE $NPM_PREFIX_JS)

if ($LASTEXITCODE -ne 0) {
  Write-Host "Could not determine Node.js install directory"
  exit 1
}

$NPM_PREFIX_NPM_CLI_JS="$NPM_PREFIX/node_modules/npm/bin/npm-cli.js"
if (Test-Path $NPM_PREFIX_NPM_CLI_JS) {
  $NPM_CLI_JS=$NPM_PREFIX_NPM_CLI_JS
}

if ($MyInvocation.ExpectingInput) { # takes pipeline input
  $input | & $NODE_EXE $NPM_CLI_JS $args
} elseif (-not $MyInvocation.Line) { # used "-File" argument
  & $NODE_EXE $NPM_CLI_JS $args
} else { # used "-Command" argument
  if (($MyInvocation | Get-Member -Name 'Statement') -and $MyInvocation.Statement) {
    $NPM_ORIGINAL_COMMAND = $MyInvocation.Statement
  } else {
    $NPM_ORIGINAL_COMMAND = (
      [Management.Automation.InvocationInfo].GetProperty('ScriptPosition', [Reflection.BindingFlags] 'Instance, NonPublic')
    ).GetValue($MyInvocation).Text
  }

  $NODE_EXE = $NODE_EXE.Replace("``", "````")
  $NPM_CLI_JS = $NPM_CLI_JS.Replace("``", "````")

  $NPM_COMMAND_ARRAY = [Management.Automation.Language.Parser]::ParseInput($NPM_ORIGINAL_COMMAND, [ref] $null, [ref] $null).
    EndBlock.Statements.PipelineElements.CommandElements.Extent.Text
  $NPM_ARGS = ($NPM_COMMAND_ARRAY | Select-Object -Skip 1) -join ' '

  Invoke-Expression "& `"$NODE_EXE`" `"$NPM_CLI_JS`" $NPM_ARGS"
}

exit $LASTEXITCODE
