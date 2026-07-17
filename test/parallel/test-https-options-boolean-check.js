'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');

if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const https = require('https');

function toArrayBuffer(buf) {
  const ab = new ArrayBuffer(buf.length);
  const view = new Uint8Array(ab);
  return buf.map((b, i) => view[i] = b);
}

function toDataView(buf) {
  const ab = new ArrayBuffer(buf.length);
  const view = new DataView(ab);
  return buf.map((b, i) => view[i] = b);
}

const keyBuff = fixtures.readKey('agent1-key.pem');
const certBuff = fixtures.readKey('agent1-cert.pem');
const keyBuff2 = fixtures.readKey('ec-key.pem');
const certBuff2 = fixtures.readKey('ec-cert.pem');
const caCert = fixtures.readKey('ca1-cert.pem');
const caCert2 = fixtures.readKey('ca2-cert.pem');
const keyStr = keyBuff.toString();
const certStr = certBuff.toString();
const keyStr2 = keyBuff2.toString();
const certStr2 = certBuff2.toString();
const caCertStr = caCert.toString();
const caCertStr2 = caCert2.toString();
const keyArrBuff = toArrayBuffer(keyBuff);
const certArrBuff = toArrayBuffer(certBuff);
const caArrBuff = toArrayBuffer(caCert);
const keyDataView = toDataView(keyBuff);
const certDataView = toDataView(certBuff);
const caArrDataView = toDataView(caCert);

function filterBoringSSLKeyCertArrayCases(options, setName) {
  if (!process.features.openssl_is_boringssl)
    return options;

  // The array-valued cases exercise multi-identity key/cert handling.
  // BoringSSL may reject those cases with backend key/cert mismatch errors
  // before the boolean/type validation this test is targeting. Keep the scalar
  // cases so https.createServer() option type validation is still covered.
  common.printSkipMessage(
    `BoringSSL: skipping ${setName} key/cert array cases`);
  return options.filter(([key, cert]) => !Array.isArray(key) &&
                                         !Array.isArray(cert));
}

// Checks to ensure https.createServer doesn't throw an error
// Format ['key', 'cert']
const validOptions = [
  [keyBuff, certBuff],
  [false, certBuff],
  [keyBuff, false],
  [keyStr, certStr],
  [false, certStr],
  [keyStr, false],
  [false, false],
  [keyArrBuff, certArrBuff],
  [keyArrBuff, false],
  [false, certArrBuff],
  [keyDataView, certDataView],
  [keyDataView, false],
  [false, certDataView],
  [[keyBuff, keyBuff2], [certBuff, certBuff2]],
  [[keyStr, keyStr2], [certStr, certStr2]],
  [[keyStr, keyStr2], false],
  [false, [certStr, certStr2]],
  [[{ pem: keyBuff }], false],
  [[{ pem: keyBuff }, { pem: keyBuff }], false],
];

filterBoringSSLKeyCertArrayCases(validOptions, 'valid')
  .forEach(([key, cert]) => {
    https.createServer({ key, cert });
  });

// Checks to ensure https.createServer predictably throws an error
// Format ['key', 'cert', 'expected message']
const invalidKeyOptions = [
  [true, certBuff],
  [true, certStr],
  [true, certArrBuff],
  [true, certDataView],
  [true, false],
  [true, false],
  [{ pem: keyBuff }, false],
  [1, false],
  [[keyBuff, true], [certBuff, certBuff2], 1],
  [[true, keyStr2], [certStr, certStr2], 0],
  [[true, false], [certBuff, certBuff2], 0],
  [true, [certBuff, certBuff2]],
];

for (const [key, cert, index] of
  filterBoringSSLKeyCertArrayCases(invalidKeyOptions, 'invalid key')) {
  const val = index === undefined ? key : key[index];
  assert.throws(() => {
    https.createServer({ key, cert });
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: 'The "options.key" property must be of type string or an ' +
             'instance of Buffer, TypedArray, or DataView.' +
             common.invalidArgTypeHelper(val)
  });
}

const invalidCertOptions = [
  [keyBuff, true],
  [keyStr, true],
  [keyArrBuff, true],
  [keyDataView, true],
  [true, true],
  [false, true],
  [false, { pem: keyBuff }],
  [false, 1],
  [[keyBuff, keyBuff2], [true, certBuff2], 0],
  [[keyStr, keyStr2], [certStr, true], 1],
  [[keyStr, keyStr2], [true, false], 0],
  [[keyStr, keyStr2], true],
];

for (const [key, cert, index] of
  filterBoringSSLKeyCertArrayCases(invalidCertOptions, 'invalid cert')) {
  const val = index === undefined ? cert : cert[index];
  assert.throws(() => {
    https.createServer({ key, cert });
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: 'The "options.cert" property must be of type string or an ' +
             'instance of Buffer, TypedArray, or DataView.' +
             common.invalidArgTypeHelper(val)
  });
}

// Checks to ensure https.createServer works with the CA parameter
// Format ['key', 'cert', 'ca']
[
  [keyBuff, certBuff, caCert],
  [keyBuff, certBuff, [caCert, caCert2]],
  [keyBuff, certBuff, caCertStr],
  [keyBuff, certBuff, [caCertStr, caCertStr2]],
  [keyBuff, certBuff, caArrBuff],
  [keyBuff, certBuff, caArrDataView],
  [keyBuff, certBuff, false],
].forEach(([key, cert, ca]) => {
  https.createServer({ key, cert, ca });
});

// Checks to ensure https.createServer throws an error for CA assignment
// Format ['key', 'cert', 'ca']
[
  [keyBuff, certBuff, true],
  [keyBuff, certBuff, {}],
  [keyBuff, certBuff, 1],
  [keyBuff, certBuff, true],
  [keyBuff, certBuff, [caCert, true], 1],
].forEach(([key, cert, ca, index]) => {
  const val = index === undefined ? ca : ca[index];
  assert.throws(() => {
    https.createServer({ key, cert, ca });
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    name: 'TypeError',
    message: 'The "options.ca" property must be of type string or an instance' +
             ' of Buffer, TypedArray, or DataView.' +
             common.invalidArgTypeHelper(val)
  });
});
