try
{
  $PSScriptRoot = Split-Path $MyInvocation.MyCommand.Path -Parent;

  # Ask for an opaque user credentials token
  $cred = Get-Credential $( whoami ) -Message "The Node.js installer needs your credentials to streamline the installation of the Build Tools";
  if (!($cred)) {
    # Wait for user confirmation.
    Echo 'The installer will now exit.';
    Read-Host 'Press ENTER to finish';
    exit 1;
  }
  # The following line is lifted verbatim from https://boxstarter.org/InstallBoxstarter
  iex ((New-Object System.Net.WebClient).DownloadString('https://boxstarter.org/bootstrapper.ps1')); get-boxstarter -Force;

  # Install the packges from `install_tools.txt`
  Install-BoxstarterPackage -PackageName ($PSScriptRoot+'\install_tools.txt') -Credential $cred;

  # Wait for user confirmation.
  Read-Host 'Press ENTER to finish';
} catch {
  exit 1;
}
