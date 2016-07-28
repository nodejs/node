# Port of tools/test-npm.sh to windows

# Handle parameters
param (
  [string]$progress = "classic",
  [string]$logfile
)

# Always change the working directory to the project's root directory
$dp0 = (Get-Item -Path ".\" -Verbose).FullName
cd $~dp0\..

# Use rmdir to get around long file path issues
Cmd /C "rmdir /S /Q test-npm"
Remove-Item test-npm.tap -ErrorAction SilentlyContinue

# Make a copy of deps/npm to run the tests on
Copy-Item deps\npm test-npm -Recurse
cd test-npm

# Do a rm first just in case deps/npm contained these
Remove-Item npm-cache -Force -Recurse -ErrorAction SilentlyContinue
Remove-Item npm-tmp -Force -Recurse -ErrorAction SilentlyContinue
Remove-Item npm-prefix -Force -Recurse -ErrorAction SilentlyContinue
Remove-Item npm-userconfig -Force -Recurse -ErrorAction SilentlyContinue
Remove-Item npm-home -Force -Recurse -ErrorAction SilentlyContinue

New-Item -ItemType directory -Path npm-cache
New-Item -ItemType directory -Path npm-tmp
New-Item -ItemType directory -Path npm-prefix
New-Item -ItemType directory -Path npm-userconfig
New-Item -ItemType directory -Path npm-home

# Set some npm env variables to point to our new temporary folders
$pwd = (Get-Item -Path ".\" -Verbose).FullName
Set-Variable -Name npm_config_cache -value ("$pwd\npm-cache") -Scope Global
Set-Variable -Name npm_config_prefix -value ("$pwd\npm-prefix") -Scope Global
Set-Variable -Name npm_config_tmp -value ("$pwd\npm-tmp") -Scope Global
Set-Variable -Name npm_config_userconfig -value ("$pwd\npm-userconfig") -Scope Global
Set-Variable -Name home -value ("$pwd\npm-home") -Scope Global -Force

# Ensure npm always uses the local node
Set-Variable -Name NODEPATH -value (Get-Item -Path "..\Release" -Verbose).FullName
$env:Path = ("$NODEPATH;$env:Path")
Remove-Variable -Name NODEPATH -ErrorAction SilentlyContinue

# Make sure the binaries from the non-dev-deps are available
node cli.js rebuild
# Install npm devDependencies and run npm's tests
node cli.js install --ignore-scripts

# Run the tests with logging if set
if ($logfile -eq $null)
{
  node cli.js run test-node -- --reporter=$progress
} else {
  node cli.js run test-node -- --reporter=$progress  2>&1 | Tee-Object -FilePath "..\$logfile"
}

# Move npm-debug.log out of test-npm so it isn't cleaned up
if (Test-Path -path "npm-debug.log") {
  Move-Item npm-debug.log ..
}

# Clean up everything in one single shot
cd ..
Cmd /C "rmdir /S /Q test-npm"
