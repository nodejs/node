# system-ca

Tests for [--use-system-ca](../../doc/api/cli.md#--use-system-ca).

On both macOS and Windows interactive dialogs need confirming to add certificates to the OS trust store.

## macOS

**Adding the certificate**

```bash
security add-trusted-cert \
  -k /Users/$USER/Library/Keychains/login.keychain-db \
  test/fixtures/keys/fake-startcom-root-cert.pem
security add-certificates \
  -k /Users/$USER/Library/Keychains/login.keychain-db \
  test/fixtures/keys/intermediate-ca.pem
security add-certificates \
  -k /Users/$USER/Library/Keychains/login.keychain-db \
  test/fixtures/keys/non-trusted-intermediate-ca.pem
```

**Removing the certificate**

```bash
security delete-certificate -c 'StartCom Certification Authority' \
  -t /Users/$USER/Library/Keychains/login.keychain-db
security delete-certificate -c 'NodeJS-Test-Intermediate-CA' \
  -t /Users/$USER/Library/Keychains/login.keychain-db
security delete-certificate -c 'NodeJS-Non-Trusted-Test-Intermediate-CA' \
  -t /Users/$USER/Library/Keychains/login.keychain-db
```

## Windows

**Adding the certificate**

Powershell:

```powershell
Import-Certificate -FilePath .\test\fixtures\keys\fake-startcom-root-cert.cer \
  -CertStoreLocation Cert:\CurrentUser\Root
Import-Certificate -FilePath .\test\fixtures\keys\intermediate-ca.pem \
  -CertStoreLocation Cert:\CurrentUser\CA
Import-Certificate -FilePath .\test\fixtures\keys\non-trusted-intermediate-ca.pem \
  -CertStoreLocation Cert:\CurrentUser\CA
```

**Removing the certificate**

```powershell
$thumbprint = (Get-ChildItem -Path Cert:\CurrentUser\Root | \
  Where-Object { $_.Subject -match "StartCom Certification Authority" }).Thumbprint
Remove-Item -Path "Cert:\CurrentUser\Root\$thumbprint"

$thumbprint = (Get-ChildItem -Path Cert:\CurrentUser\CA | \
  Where-Object { $_.Subject -match "NodeJS-Test-Intermediate-CA" }).Thumbprint
Remove-Item -Path "Cert:\CurrentUser\CA\$thumbprint"

$thumbprint = (Get-ChildItem -Path Cert:\CurrentUser\CA | \
  Where-Object { $_.Subject -match "NodeJS-Non-Trusted-Test-Intermediate-CA" }).Thumbprint
Remove-Item -Path "Cert:\CurrentUser\CA\$thumbprint"
```

## Debian/Ubuntu

**Adding the certificate**

```bash
sudo cp test/fixtures/keys/fake-startcom-root-cert.pem \
  /usr/local/share/ca-certificates/fake-startcom-root-cert.crt
sudo cp test/fixtures/keys/intermediate-ca.pem \
  /usr/local/share/ca-certificates/intermediate-ca.crt
sudo cp test/fixtures/keys/non-trusted-intermediate-ca.pem \
  /usr/local/share/ca-certificates/non-trusted-intermediate-ca.crt
sudo update-ca-certificates
```

**Removing the certificate**

```bash
sudo rm /usr/local/share/ca-certificates/fake-startcom-root-cert.crt \
  /usr/local/share/ca-certificates/intermediate-ca.crt \
  /usr/local/share/ca-certificates/non-trusted-intermediate-ca.crt
sudo update-ca-certificates --fresh
```

## Other Unix-like systems

For other Unix-like systems, consult their manuals, there are usually
file-based processes similar to the Debian/Ubuntu one but with different
file locations and update commands.
