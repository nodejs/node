const assert = require('assert');
const EXPECTED_CERTS_PATH = process.env.EXPECTED_CERTS_PATH;
let expectedCerts = [];
if (EXPECTED_CERTS_PATH) {
  const fs = require('fs');
  const file = fs.readFileSync(EXPECTED_CERTS_PATH, 'utf-8');
  const expectedCerts = file.split('-----END CERTIFICATE-----\n')
    .filter(line => line.trim() !== '')
    .map(line => line + '-----END CERTIFICATE-----\n');
}

const tls = require('tls');
const { includesCert, extractMetadata } = require('../common/tls');

const CERTS_TYPE = process.env.CERTS_TYPE || 'default';
const actualCerts = tls.getCACertificates(CERTS_TYPE);
for (const cert of expectedCerts) {
  assert(includesCert(actualCerts, cert), 'Expected certificate not found: ' + JSON.stringify(extractMetadata(cert)));
}
