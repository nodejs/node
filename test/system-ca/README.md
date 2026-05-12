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
security add-trusted-cert \
  -k /Users/$USER/Library/Keychains/login.keychain-db \
  test/fixtures/keys/expired-root-cert.pem
# Self-signed cert with trust settings that lack kSecTrustSettingsResult.
security add-trusted-cert \
  -k /Users/$USER/Library/Keychains/login.keychain-db \
  test/fixtures/keys/selfsigned-no-result-root-cert.pem
security trust-settings-export /tmp/node-trust-settings.plist
CERT_SHA1=$(openssl x509 \
  -in test/fixtures/keys/selfsigned-no-result-root-cert.pem \
  -fingerprint -sha1 -noout | sed 's/.*=//;s/://g')
/usr/libexec/PlistBuddy \
  -c "Delete :trustList:${CERT_SHA1}:trustSettings" \
  /tmp/node-trust-settings.plist
/usr/libexec/PlistBuddy \
  -c "Add :trustList:${CERT_SHA1}:trustSettings array" \
  /tmp/node-trust-settings.plist
/usr/libexec/PlistBuddy \
  -c "Add :trustList:${CERT_SHA1}:trustSettings:0 dict" \
  /tmp/node-trust-settings.plist
security trust-settings-import /tmp/node-trust-settings.plist
rm /tmp/node-trust-settings.plist
# Duplicate cert in a second keychain
security create-keychain -p "test" /tmp/node-test-dup.keychain
security add-certificates \
  -k /tmp/node-test-dup.keychain \
  test/fixtures/keys/fake-startcom-root-cert.pem
security list-keychains -d user -s login.keychain-db /tmp/node-test-dup.keychain
```

**Removing the certificate**

```bash
security delete-certificate -c 'StartCom Certification Authority' \
  -t /Users/$USER/Library/Keychains/login.keychain-db
security delete-certificate -c 'NodeJS-Test-Intermediate-CA' \
  -t /Users/$USER/Library/Keychains/login.keychain-db
security delete-certificate -c 'NodeJS-Non-Trusted-Test-Intermediate-CA' \
  -t /Users/$USER/Library/Keychains/login.keychain-db
security delete-certificate -c 'NodeJS-Test-Expired-Root' \
  -t /Users/$USER/Library/Keychains/login.keychain-db
security delete-certificate -c 'NodeJS-Test-No-Result-Root' \
  -t /Users/$USER/Library/Keychains/login.keychain-db
security list-keychains -d user -s login.keychain-db
security delete-keychain /tmp/node-test-dup.keychain
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
