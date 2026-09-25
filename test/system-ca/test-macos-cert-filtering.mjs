// Flags: --use-system-ca

import * as common from '../common/index.mjs';
import assert from 'node:assert/strict';
import * as fixtures from '../common/fixtures.mjs';
import { it, describe } from 'node:test';
import { includesCert, extractMetadata } from '../common/tls.js';
import { execFileSync } from 'node:child_process';

if (!common.hasCrypto) {
  common.skip('requires crypto');
}

if (process.platform !== 'darwin') {
  common.skip('macOS-specific test');
}

function isCertInKeychain(cn) {
  try {
    execFileSync('security', ['find-certificate', '-c', cn], { stdio: 'pipe' });
    return true;
  } catch {
    return false;
  }
}

function isDupKeychainPresent() {
  try {
    const out = execFileSync(
      'security', ['list-keychains', '-d', 'user'],
      { encoding: 'utf8' },
    );
    return out.includes('node-test-dup.keychain');
  } catch {
    return false;
  }
}

const { default: tls } = await import('node:tls');

const systemCerts = tls.getCACertificates('system');
const fakeStartcomCert = fixtures.readKey('fake-startcom-root-cert.pem');
if (!includesCert(systemCerts, fakeStartcomCert)) {
  common.skip(
    'fake-startcom-root-cert.pem not found in system CA store. ' +
    'Please follow setup instructions in test/system-ca/README.md',
  );
}
if (!isDupKeychainPresent()) {
  common.skip(
    'Duplicate keychain not set up. ' +
    'Please follow setup instructions in test/system-ca/README.md',
  );
}
if (!isCertInKeychain('NodeJS-Test-Expired-Root')) {
  common.skip(
    'Expired cert not installed. ' +
    'Please follow setup instructions in test/system-ca/README.md',
  );
}

describe('macOS certificate filtering', () => {
  it('includes self-signed cert with absent kSecTrustSettingsResult', () => {
    const noResultCert = fixtures.readKey('selfsigned-no-result-root-cert.pem');
    assert.ok(
      includesCert(systemCerts, noResultCert),
      'Self-signed cert with absent kSecTrustSettingsResult ' +
      '(defaulting to TrustRoot) should be in system CA list',
    );
  });

  it('deduplicates certificates from multiple keychains', () => {
    const target = extractMetadata(fakeStartcomCert);
    const matches = systemCerts.filter((c) => {
      const m = extractMetadata(c);
      return m.serialNumber === target.serialNumber &&
             m.issuer === target.issuer &&
             m.subject === target.subject;
    });
    assert.strictEqual(
      matches.length, 1,
      `Expected exactly 1 copy of fake-startcom-root-cert, found ${matches.length}`,
    );
  });

  it('filters out expired certificates', () => {
    const expiredCert = fixtures.readKey('expired-root-cert.pem');
    assert.ok(
      !includesCert(systemCerts, expiredCert),
      'Expired certificate should not be in system CA list',
    );
  });
});
