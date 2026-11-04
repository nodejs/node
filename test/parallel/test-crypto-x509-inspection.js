'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');

const assert = require('assert');
const { X509Certificate } = require('crypto');
const { isBoringSSL } = require('../common/crypto');
const fixtures = require('../common/fixtures');

function load(name) {
  return new X509Certificate(fixtures.readKey(name));
}

function readDer(input, offset = 0) {
  const start = offset;
  const tag = input[offset++];
  let length = input[offset++];
  if ((length & 0x80) !== 0) {
    const octets = length & 0x7f;
    length = 0;
    for (let i = 0; i < octets; i++)
      length = (length * 256) + input[offset++];
  }
  return {
    tag,
    start,
    contentStart: offset,
    end: offset + length,
  };
}

function readChildren(input, parent) {
  const children = [];
  for (let offset = parent.contentStart; offset < parent.end;) {
    const child = readDer(input, offset);
    children.push(child);
    offset = child.end;
  }
  return children;
}

function encodeDer(tag, content) {
  let length;
  if (content.length < 128) {
    length = Buffer.of(content.length);
  } else {
    const octets = [];
    for (let value = content.length; value !== 0; value >>>= 8)
      octets.unshift(value & 0xff);
    length = Buffer.of(0x80 | octets.length, ...octets);
  }
  return Buffer.concat([Buffer.of(tag), length, content]);
}

function rawElement(input, element) {
  return input.subarray(element.start, element.end);
}

function appendExtensions(raw, additions) {
  const certificate = readDer(raw);
  const certificateFields = readChildren(raw, certificate);
  const tbs = certificateFields[0];
  const tbsFields = readChildren(raw, tbs);
  const extensionIndex = tbsFields.findIndex(({ tag }) => tag === 0xa3);
  assert.notStrictEqual(extensionIndex, -1);

  const wrapper = tbsFields[extensionIndex];
  const extensionSequence = readChildren(raw, wrapper)[0];
  const extensions = readChildren(raw, extensionSequence)
    .map((element) => rawElement(raw, element));
  extensions.push(...additions);

  const newWrapper = encodeDer(0xa3, encodeDer(
    0x30, Buffer.concat(extensions)));
  const newTbsFields = tbsFields.map((element, index) =>
    (index === extensionIndex ? newWrapper : rawElement(raw, element)));
  const newTbs = encodeDer(0x30, Buffer.concat(newTbsFields));
  const newCertificateFields = certificateFields.map((element, index) =>
    (index === 0 ? newTbs : rawElement(raw, element)));
  return encodeDer(0x30, Buffer.concat(newCertificateFields));
}

function replaceVersion(raw, value) {
  const certificate = readDer(raw);
  const certificateFields = readChildren(raw, certificate);
  const tbs = certificateFields[0];
  const tbsFields = readChildren(raw, tbs);
  assert.strictEqual(tbsFields[0].tag, 0xa0);
  const version = encodeDer(0xa0, encodeDer(0x02, value));
  const newTbs = encodeDer(0x30, Buffer.concat(tbsFields.map(
    (element, index) => (index === 0 ? version : rawElement(raw, element)))));
  return encodeDer(0x30, Buffer.concat(certificateFields.map(
    (element, index) => (index === 0 ? newTbs : rawElement(raw, element)))));
}

function replaceSubject(raw, value) {
  const certificate = readDer(raw);
  const certificateFields = readChildren(raw, certificate);
  const tbs = certificateFields[0];
  const tbsFields = readChildren(raw, tbs);
  const fieldOffset = tbsFields[0].tag === 0xa0 ? 1 : 0;
  const subjectIndex = fieldOffset + 4;
  const newTbs = encodeDer(0x30, Buffer.concat(tbsFields.map(
    (element, index) =>
      (index === subjectIndex ? value : rawElement(raw, element)))));
  return encodeDer(0x30, Buffer.concat(certificateFields.map(
    (element, index) => (index === 0 ? newTbs : rawElement(raw, element)))));
}

const keyUsageExtension = Buffer.from(
  '300b0603551d0f0404030205a0', 'hex');
const unknownExtension = Buffer.from(
  '300906032a030404020500', 'hex');
const criticalUnknownExtension = Buffer.from(
  '300c06032a03040101ff04020500', 'hex');

{
  const cert = load('agent1-cert.pem');
  const raw = cert.raw;
  const certificate = readDer(raw);
  const [tbs] = readChildren(raw, certificate);
  const tbsFields = readChildren(raw, tbs);
  const fieldOffset = tbsFields[0].tag === 0xa0 ? 1 : 0;
  const issuer = tbsFields[fieldOffset + 2];
  const subject = tbsFields[fieldOffset + 4];

  assert.strictEqual(cert.version, 3);
  assert.strictEqual(cert.subjectName.rdns.at(-1)[0].oid,
                     '1.2.840.113549.1.9.1');
  assert.strictEqual(cert.subjectName.rdns.at(-1)[0].value,
                     'ry@tinyclouds.org');
  assert.deepStrictEqual(cert.subjectName.raw, rawElement(raw, subject));
  assert.strictEqual(cert.issuerName.rdns.at(-2)[0].value, 'ca1');
  assert.deepStrictEqual(cert.issuerName.raw, rawElement(raw, issuer));

  assert.deepStrictEqual(cert.extensions.map(({ oid, critical }) => ({
    oid,
    critical,
  })), [{
    oid: '1.3.6.1.5.5.7.1.1',
    critical: false,
  }]);
  assert(cert.extensions[0].value.length > 0);

  assert.deepStrictEqual(cert.tbs, rawElement(raw, tbs));

  assert.deepStrictEqual(cert.signature.algorithm, {
    oid: '1.2.840.113549.1.1.11',
    parameters: Buffer.from('0500', 'hex'),
  });
  assert(cert.signature.value.length > 0);
}

// Raw inspection preserves extension order, duplicates, criticality, and the
// exact bytes inside extnValue without interpreting the extension syntax.
{
  const source = load('agent1-cert.pem');
  const cert = new X509Certificate(appendExtensions(source.raw, [
    keyUsageExtension,
    unknownExtension,
    criticalUnknownExtension,
  ]));

  assert.deepStrictEqual(cert.extensions.slice(-3), [
    {
      oid: '2.5.29.15',
      critical: false,
      value: Buffer.from('030205a0', 'hex'),
    },
    {
      oid: '1.2.3.4',
      critical: false,
      value: Buffer.from('0500', 'hex'),
    },
    {
      oid: '1.2.3.4',
      critical: true,
      value: Buffer.from('0500', 'hex'),
    },
  ]);
}

// An empty Name is valid, but an RDN is a nonempty SET OF attributes. OpenSSL
// does not expose empty RDNs through X509_NAME entries, so inspection must not
// silently collapse this malformed Name into a valid empty one.
{
  const source = load('agent1-cert.pem');
  const emptyRdnName = encodeDer(0x30, encodeDer(0x31, Buffer.alloc(0)));
  const malformed = replaceSubject(source.raw, emptyRdnName);
  let cert;
  try {
    cert = new X509Certificate(malformed);
  } catch (err) {
    if (!isBoringSSL || err.code !== 'ERR_OSSL_PEM_NO_START_LINE') {
      throw err;
    }
  }
  if (cert !== undefined) {
    assert.throws(() => cert.subjectName, {
      code: 'ERR_CRYPTO_INVALID_X509_CERTIFICATE',
    });
  }
}

// Reject unsupported versions at whichever native parsing layer observes them
// first. Backends that accept the encoding must reject it before converting
// OpenSSL's long result to the public one-based version number.
{
  const source = load('agent1-cert.pem');
  let cert;
  try {
    cert = new X509Certificate(replaceVersion(
      source.raw, Buffer.from('7fffffffffffffff', 'hex')));
  } catch (err) {
    assert.strictEqual(err.code, isBoringSSL ?
      'ERR_OSSL_PEM_NO_START_LINE' : 'ERR_OSSL_X509_INVALID_VERSION');
  }
  if (cert !== undefined) {
    assert.throws(() => cert.version, {
      code: 'ERR_CRYPTO_INVALID_X509_CERTIFICATE',
    });
  }
}

// Inspection uses the immutable native snapshot, not the mutable Buffer cached
// by the legacy raw getter.
{
  const cert = load('agent1-cert.pem');
  cert.raw.fill(0);
  assert.strictEqual(cert.version, 3);
  assert.strictEqual(cert.subjectName.rdns.at(-1)[0].value,
                     'ry@tinyclouds.org');
}

// New structured getters return fresh defensive values. Existing legacy
// getters retain their current identity behavior in this backportable change.
{
  const cert = load('leaf-from-intermediate-cert.pem');
  const subjectName = cert.subjectName;
  const subjectRaw = subjectName.raw;
  const extensions = cert.extensions;
  const tbs = cert.tbs;
  const signature = cert.signature;
  const parameters = signature.algorithm.parameters;

  subjectName.rdns.length = 0;
  subjectRaw.fill(0);
  extensions.length = 0;
  tbs.fill(0);
  signature.value.fill(0);
  parameters.fill(0);

  assert(cert.subjectName.rdns.length > 0);
  assert.notDeepStrictEqual(cert.subjectName.raw, subjectRaw);
  assert(cert.extensions.length > 0);
  assert.notStrictEqual(cert.tbs[0], 0);
  assert.notStrictEqual(cert.signature.value[0], 0);
  assert.notStrictEqual(cert.signature.algorithm.parameters[0], 0);
  assert.strictEqual(cert.raw, cert.raw);
}

for (const name of [
  'version',
  'subjectName',
  'issuerName',
  'extensions',
  'tbs',
  'signature',
]) {
  const descriptor = Object.getOwnPropertyDescriptor(
    X509Certificate.prototype, name);
  assert.strictEqual(typeof descriptor.get, 'function');
  assert.strictEqual(descriptor.set, undefined);
  assert.strictEqual(descriptor.enumerable, false);
  assert.strictEqual(descriptor.configurable, true);
  assert.throws(() => descriptor.get.call({}), {
    code: 'ERR_INVALID_THIS',
  });
}
