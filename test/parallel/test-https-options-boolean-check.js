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
const invalidKeyRE = /^The "key" argument must be one of type string, Buffer, TypedArray, or DataView$/;
const invalidCertRE = /^The "cert" argument must be one of type string, Buffer, TypedArray, or DataView$/;

// Checks to ensure https.createServer doesn't throw an error
// Format ['key', 'cert']
[
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
  [[{ pem: keyBuff }, { pem: keyBuff }], false]
].forEach((params) => {
  assert.doesNotThrow(() => {
    https.createServer({
      key: params[0],
      cert: params[1]
    });
  });
});

// Checks to ensure https.createServer predictably throws an error
// Format ['key', 'cert', 'expected message']
[
  [true, certBuff, invalidKeyRE],
  [keyBuff, true, invalidCertRE],
  [true, certStr, invalidKeyRE],
  [keyStr, true, invalidCertRE],
  [true, certArrBuff, invalidKeyRE],
  [keyArrBuff, true, invalidCertRE],
  [true, certDataView, invalidKeyRE],
  [keyDataView, true, invalidCertRE],
  [true, true, invalidCertRE],
  [true, false, invalidKeyRE],
  [false, true, invalidCertRE],
  [true, false, invalidKeyRE],
  [{ pem: keyBuff }, false, invalidKeyRE],
  [false, { pem: keyBuff }, invalidCertRE],
  [1, false, invalidKeyRE],
  [false, 1, invalidCertRE],
  [[keyBuff, true], [certBuff, certBuff2], invalidKeyRE],
  [[true, keyStr2], [certStr, certStr2], invalidKeyRE],
  [[keyBuff, keyBuff2], [true, certBuff2], invalidCertRE],
  [[keyStr, keyStr2], [certStr, true], invalidCertRE],
  [[true, false], [certBuff, certBuff2], invalidKeyRE],
  [[keyStr, keyStr2], [true, false], invalidCertRE],
  [[keyStr, keyStr2], true, invalidCertRE],
  [true, [certBuff, certBuff2], invalidKeyRE]
].forEach((params) => {
  common.expectsError(() => {
    https.createServer({
      key: params[0],
      cert: params[1]
    });
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: params[2]
  });
});

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
].forEach((params) => {
  assert.doesNotThrow(() => {
    https.createServer({
      key: params[0],
      cert: params[1],
      ca: params[2]
    });
  });
});

// Checks to ensure https.createServer throws an error for CA assignment
// Format ['key', 'cert', 'ca']
[
  [keyBuff, certBuff, true],
  [keyBuff, certBuff, {}],
  [keyBuff, certBuff, 1],
  [keyBuff, certBuff, true],
  [keyBuff, certBuff, [caCert, true]]
].forEach((params) => {
  common.expectsError(() => {
    https.createServer({
      key: params[0],
      cert: params[1],
      ca: params[2]
    });
  }, {
    code: 'ERR_INVALID_ARG_TYPE',
    type: TypeError,
    message: /^The "ca" argument must be one of type string, Buffer, TypedArray, or DataView$/
  });
});
