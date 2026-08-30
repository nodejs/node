'use strict';

const {
  ArrayIsArray,
  ArrayPrototypePush,
  Date,
  DatePrototypeGetTime,
  DatePrototypeToISOString,
  FunctionPrototypeCall,
  MathFloor,
  Number,
  NumberIsFinite,
  NumberParseInt,
  ObjectEntries,
  ObjectPrototypeHasOwnProperty,
  SafeSet,
  StringPrototypeCharCodeAt,
  StringPrototypeIncludes,
  StringPrototypeSlice,
  Uint8Array,
} = primordials;

const { Buffer } = require('buffer');

const {
  parseX509CertificateRequest,
  X509GenerateJob,
  kCryptoJobAsync,
  kCryptoJobSync,
} = internalBinding('crypto');

const {
  preparePrivateKey,
  preparePublicOrPrivateKey,
} = require('internal/crypto/keys');
const {
  getPadding,
  getSaltLength,
} = require('internal/crypto/sig');

const {
  X509Certificate,
  isX509Certificate,
} = require('internal/crypto/x509');

const {
  isAnyArrayBuffer,
  isArrayBufferView,
} = require('internal/util/types');
const { isIP } = require('internal/net');

const {
  validateArray,
  validateBoolean,
  validateFunction,
  validateInteger,
  validateObject,
  validateString,
} = require('internal/validators');

const {
  codes: {
    ERR_INCOMPATIBLE_OPTION_PAIR,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_MISSING_OPTION,
    ERR_OUT_OF_RANGE,
  },
} = require('internal/errors');

const kGeneralNameDns = 0;
const kGeneralNameIp = 1;
const kGeneralNameRfc822 = 2;
const kGeneralNameUri = 4;

const kExtensionRaw = 0;
const kExtensionBasicConstraints = 1;
const kExtensionKeyUsage = 2;
const kExtensionExtendedKeyUsage = 3;
const kExtensionSubjectAltName = 4;
const kExtensionSubjectKeyIdentifier = 5;
const kExtensionAuthorityKeyIdentifier = 6;

const kNameAttributeNames = {
  '__proto__': null,
  '2.5.4.3': ['commonName', 'CN'],
  '2.5.4.6': ['countryName', 'C'],
  '2.5.4.7': ['localityName', 'L'],
  '2.5.4.8': ['stateOrProvinceName', 'ST'],
  '2.5.4.10': ['organizationName', 'O'],
  '2.5.4.11': ['organizationalUnitName', 'OU'],
  '2.5.4.5': 'serialNumber',
  '2.5.4.9': 'streetAddress',
  '2.5.4.17': 'postalCode',
  '0.9.2342.19200300.100.1.25': ['domainComponent', 'DC'],
  '0.9.2342.19200300.100.1.1': ['userId', 'UID'],
  '1.2.840.113549.1.9.1': ['emailAddress', 'E'],
  '2.5.4.12': 'title',
  '2.5.4.4': ['surname', 'SN'],
  '2.5.4.42': ['givenName', 'GN'],
};
const kNameAttributes = { '__proto__': null };
const nameAttributeEntries = ObjectEntries(kNameAttributeNames);
for (let i = 0; i < nameAttributeEntries.length; i++) {
  const { 0: oid, 1: names } = nameAttributeEntries[i];
  if (ArrayIsArray(names)) {
    for (let j = 0; j < names.length; j++)
      kNameAttributes[names[j]] = oid;
  } else {
    kNameAttributes[names] = oid;
  }
}

const kKeyUsageBits = {
  '__proto__': null,
  'digitalSignature': 0,
  'contentCommitment': 1,
  'keyEncipherment': 2,
  'dataEncipherment': 3,
  'keyAgreement': 4,
  'keyCertSign': 5,
  'crlSign': 6,
  'encipherOnly': 7,
  'decipherOnly': 8,
};

const kExtendedKeyUsageOids = {
  '__proto__': null,
  'serverAuth': '1.3.6.1.5.5.7.3.1',
  'clientAuth': '1.3.6.1.5.5.7.3.2',
  'codeSigning': '1.3.6.1.5.5.7.3.3',
  'emailProtection': '1.3.6.1.5.5.7.3.4',
  'timeStamping': '1.3.6.1.5.5.7.3.8',
  'ocspSigning': '1.3.6.1.5.5.7.3.9',
  'anyExtendedKeyUsage': '2.5.29.37.0',
};

function getRequired(object, property, name) {
  const value = object[property];
  if (value === undefined &&
      !ObjectPrototypeHasOwnProperty(object, property)) {
    throw new ERR_MISSING_OPTION(name);
  }
  return value;
}

function snapshotBinary(value, name) {
  if (!isAnyArrayBuffer(value) && !isArrayBufferView(value)) {
    throw new ERR_INVALID_ARG_TYPE(
      name,
      ['ArrayBuffer', 'SharedArrayBuffer', 'Buffer', 'TypedArray', 'DataView'],
      value);
  }
  const view = isAnyArrayBuffer(value) ?
    new Uint8Array(value) :
    new Uint8Array(value.buffer, value.byteOffset, value.byteLength);
  return Buffer.from(view);
}

function snapshotCertificateRequest(value) {
  if (typeof value === 'string') return Buffer.from(value);
  if (!isAnyArrayBuffer(value) && !isArrayBufferView(value)) {
    throw new ERR_INVALID_ARG_TYPE(
      'options.csr',
      [
        'string',
        'ArrayBuffer',
        'SharedArrayBuffer',
        'Buffer',
        'TypedArray',
        'DataView',
      ],
      value);
  }
  const view = isAnyArrayBuffer(value) ?
    new Uint8Array(value) :
    new Uint8Array(value.buffer, value.byteOffset, value.byteLength);
  return Buffer.from(view);
}

function validateUnicode(value, name) {
  validateString(value, name);
  for (let i = 0; i < value.length; i++) {
    const first = StringPrototypeCharCodeAt(value, i);
    if (first >= 0xd800 && first <= 0xdbff) {
      const second = StringPrototypeCharCodeAt(value, ++i);
      if (!(second >= 0xdc00 && second <= 0xdfff)) {
        throw new ERR_INVALID_ARG_VALUE(
          name, value, 'contains an unpaired surrogate');
      }
    } else if (first >= 0xdc00 && first <= 0xdfff) {
      throw new ERR_INVALID_ARG_VALUE(
        name, value, 'contains an unpaired surrogate');
    }
  }
}

function validateAsciiWithoutNul(value, name) {
  validateString(value, name);
  for (let i = 0; i < value.length; i++) {
    const character = StringPrototypeCharCodeAt(value, i);
    if (character === 0 || character > 0x7f) {
      throw new ERR_INVALID_ARG_VALUE(
        name, value, 'must contain only ASCII characters without NUL');
    }
  }
}

function validateGeneralNameText(value, name) {
  validateAsciiWithoutNul(value, name);
  if (value.length === 0) {
    throw new ERR_INVALID_ARG_VALUE(name, value, 'must not be empty');
  }
}

function normalizeNameAttribute(attribute, name) {
  validateObject(attribute, name);
  const attributeName = getRequired(attribute, 'name', `${name}.name`);
  const value = getRequired(attribute, 'value', `${name}.value`);
  validateString(attributeName, `${name}.name`);

  let oid = attributeName;
  if (ObjectPrototypeHasOwnProperty(kNameAttributes, attributeName)) {
    oid = kNameAttributes[attributeName];
  }
  validateUnicode(value, `${name}.value`);

  return [oid, value];
}

function normalizeDistinguishedName(name, path) {
  if (!ArrayIsArray(name)) {
    if (!isAnyArrayBuffer(name) && !isArrayBufferView(name)) {
      throw new ERR_INVALID_ARG_TYPE(
        path,
        [
          'Array',
          'ArrayBuffer',
          'SharedArrayBuffer',
          'Buffer',
          'TypedArray',
          'DataView',
        ],
        name);
    }
    return snapshotBinary(name, path);
  }

  validateArray(name, path);
  const rdns = [];
  for (let i = 0; i < name.length; i++) {
    const entryPath = `${path}[${i}]`;
    const entry = name[i];
    const attributes = ArrayIsArray(entry) ? entry : [entry];
    validateArray(attributes, entryPath, 1);
    const normalized = [];
    for (let j = 0; j < attributes.length; j++) {
      ArrayPrototypePush(normalized, normalizeNameAttribute(
        attributes[j], ArrayIsArray(entry) ? `${entryPath}[${j}]` : entryPath));
    }
    ArrayPrototypePush(rdns, normalized);
  }
  return rdns;
}

function serialBytes(value) {
  if (typeof value !== 'bigint')
    throw new ERR_INVALID_ARG_TYPE('options.serialNumber', 'bigint', value);
  const maximum = (1n << 159n) - 1n;
  if (value < 1n || value > maximum) {
    throw new ERR_OUT_OF_RANGE(
      'options.serialNumber', '>= 1n and <= (1n << 159n) - 1n', value);
  }
  const bytes = [];
  for (; value !== 0n; value >>= 8n)
    ArrayPrototypePush(bytes, Number(value & 0xffn));
  const output = Buffer.allocUnsafe(bytes.length);
  for (let i = 0; i < bytes.length; i++)
    output[i] = bytes[bytes.length - i - 1];
  return output;
}

function formatTime(value, name) {
  let milliseconds;
  try {
    milliseconds = DatePrototypeGetTime(value);
  } catch {
    throw new ERR_INVALID_ARG_TYPE(name, 'Date', value);
  }
  if (!NumberIsFinite(milliseconds))
    throw new ERR_INVALID_ARG_VALUE(name, value, 'must be a valid Date');
  milliseconds = MathFloor(milliseconds / 1000) * 1000;
  const iso = DatePrototypeToISOString(new Date(milliseconds));
  if (iso[0] === '+' || iso[0] === '-')
    throw new ERR_OUT_OF_RANGE(name, 'between 1950 and 9999', value);
  const year = NumberParseInt(StringPrototypeSlice(iso, 0, 4), 10);
  if (year < 1950 || year > 9999)
    throw new ERR_OUT_OF_RANGE(name, 'between 1950 and 9999', value);
  const suffix = `${StringPrototypeSlice(iso, 5, 7)}` +
    `${StringPrototypeSlice(iso, 8, 10)}` +
    `${StringPrototypeSlice(iso, 11, 13)}` +
    `${StringPrototypeSlice(iso, 14, 16)}` +
    `${StringPrototypeSlice(iso, 17, 19)}Z`;
  return {
    milliseconds,
    value: year <= 2049 ?
      `${StringPrototypeSlice(iso, 2, 4)}${suffix}` :
      `${StringPrototypeSlice(iso, 0, 4)}${suffix}`,
  };
}

function normalizeGeneralName(generalName, name) {
  validateObject(generalName, name);
  const type = getRequired(generalName, 'type', `${name}.type`);
  const value = getRequired(generalName, 'value', `${name}.value`);
  validateString(type, `${name}.type`);

  switch (type) {
    case 'dns':
      validateGeneralNameText(value, `${name}.value`);
      return [kGeneralNameDns, value];
    case 'ip': {
      validateString(value, `${name}.value`);
      if (StringPrototypeIncludes(value, '%')) {
        throw new ERR_INVALID_ARG_VALUE(
          `${name}.value`, value, 'must not include a zone identifier');
      }
      const family = isIP(value);
      if (family !== 4 && family !== 6) {
        throw new ERR_INVALID_ARG_VALUE(
          `${name}.value`, value, 'must be an IPv4 or IPv6 address');
      }
      return [kGeneralNameIp, value];
    }
    case 'email':
      validateGeneralNameText(value, `${name}.value`);
      return [kGeneralNameRfc822, value];
    case 'uri':
      validateGeneralNameText(value, `${name}.value`);
      return [kGeneralNameUri, value];
    default:
      throw new ERR_INVALID_ARG_VALUE(`${name}.type`, type);
  }
}

function normalizeGeneralNames(names, path) {
  validateArray(names, path, 1);
  const normalized = [];
  for (let i = 0; i < names.length; i++) {
    ArrayPrototypePush(
      normalized, normalizeGeneralName(names[i], `${path}[${i}]`));
  }
  return normalized;
}

function getCritical(extension, name) {
  const critical = extension.critical;
  if (critical !== undefined) validateBoolean(critical, `${name}.critical`);
  return critical === true;
}

function normalizeKeyUsage(usages, path) {
  validateArray(usages, path, 1);
  const seen = new SafeSet();
  let mask = 0;
  for (let i = 0; i < usages.length; i++) {
    const usage = usages[i];
    validateString(usage, `${path}[${i}]`);
    if (!ObjectPrototypeHasOwnProperty(kKeyUsageBits, usage))
      throw new ERR_INVALID_ARG_VALUE(`${path}[${i}]`, usage);
    if (seen.has(usage)) {
      throw new ERR_INVALID_ARG_VALUE(
        `${path}[${i}]`, usage, 'must not be duplicated');
    }
    seen.add(usage);
    mask |= 1 << kKeyUsageBits[usage];
  }
  return mask;
}

function normalizeExtendedKeyUsage(usages, path) {
  validateArray(usages, path, 1);
  const seen = new SafeSet();
  const normalized = [];
  for (let i = 0; i < usages.length; i++) {
    const value = usages[i];
    validateString(value, `${path}[${i}]`);
    const oid = ObjectPrototypeHasOwnProperty(kExtendedKeyUsageOids, value) ?
      kExtendedKeyUsageOids[value] : value;
    if (seen.has(oid)) {
      throw new ERR_INVALID_ARG_VALUE(
        `${path}[${i}]`, value, 'must not be duplicated');
    }
    seen.add(oid);
    ArrayPrototypePush(normalized, oid);
  }
  return normalized;
}

function normalizeExtensions(extensions, arrayPath = 'options.extensions') {
  if (extensions === undefined) extensions = [];
  validateArray(extensions, arrayPath);
  const output = [];

  for (let i = 0; i < extensions.length; i++) {
    const path = `${arrayPath}[${i}]`;
    const extension = extensions[i];
    validateObject(extension, path);
    const type = getRequired(extension, 'type', `${path}.type`);
    validateString(type, `${path}.type`);
    let normalized;

    switch (type) {
      case 'basicConstraints': {
        const ca = getRequired(extension, 'ca', `${path}.ca`);
        validateBoolean(ca, `${path}.ca`);
        const pathLength = extension.pathLength;
        if (pathLength !== undefined)
          validateInteger(pathLength, `${path}.pathLength`, 0);
        normalized = [
          kExtensionBasicConstraints,
          getCritical(extension, path),
          ca,
          pathLength,
        ];
        break;
      }
      case 'keyUsage':
        normalized = [
          kExtensionKeyUsage,
          getCritical(extension, path),
          normalizeKeyUsage(
            getRequired(extension, 'usages', `${path}.usages`),
            `${path}.usages`),
        ];
        break;
      case 'extendedKeyUsage':
        normalized = [
          kExtensionExtendedKeyUsage,
          getCritical(extension, path),
          normalizeExtendedKeyUsage(
            getRequired(extension, 'usages', `${path}.usages`),
            `${path}.usages`),
        ];
        break;
      case 'subjectAltName': {
        const critical = getCritical(extension, path);
        const names = normalizeGeneralNames(
          getRequired(extension, 'names', `${path}.names`),
          `${path}.names`);
        normalized = [kExtensionSubjectAltName, critical, names];
        break;
      }
      case 'subjectKeyIdentifier':
      case 'authorityKeyIdentifier': {
        const value = getRequired(extension, 'value', `${path}.value`);
        normalized = [
          type === 'subjectKeyIdentifier' ?
            kExtensionSubjectKeyIdentifier :
            kExtensionAuthorityKeyIdentifier,
          false,
          value === 'auto' ? undefined : snapshotBinary(value, `${path}.value`),
        ];
        break;
      }
      case 'raw': {
        const oid = getRequired(extension, 'oid', `${path}.oid`);
        validateString(oid, `${path}.oid`);
        normalized = [
          kExtensionRaw,
          oid,
          getCritical(extension, path),
          snapshotBinary(
            getRequired(extension, 'value', `${path}.value`), `${path}.value`),
        ];
        break;
      }
      default:
        throw new ERR_INVALID_ARG_VALUE(`${path}.type`, type);
    }

    ArrayPrototypePush(output, normalized);
  }
  return output;
}

function normalizeOptions(options, mode) {
  validateObject(options, 'options');
  let csr;
  let subject;
  let publicKeyData;
  let publicKeyFormat;
  let publicKeyType;
  let publicKeyPassphrase;
  let publicKeyNamedCurve;
  if (options.csr !== undefined) {
    if (options.subject !== undefined) {
      throw new ERR_INCOMPATIBLE_OPTION_PAIR('csr', 'subject');
    }
    if (options.publicKey !== undefined) {
      throw new ERR_INCOMPATIBLE_OPTION_PAIR('csr', 'publicKey');
    }
    if (options.extensions !== undefined) {
      throw new ERR_INCOMPATIBLE_OPTION_PAIR('csr', 'extensions');
    }
    csr = snapshotCertificateRequest(options.csr);
  } else {
    if (options.selectExtensions !== undefined) {
      throw new ERR_MISSING_OPTION('options.csr');
    }
    subject = normalizeDistinguishedName(
      getRequired(options, 'subject', 'options.subject'), 'options.subject');
    ({
      data: publicKeyData,
      format: publicKeyFormat,
      type: publicKeyType,
      passphrase: publicKeyPassphrase,
      namedCurve: publicKeyNamedCurve,
    } = preparePublicOrPrivateKey(
      getRequired(options, 'publicKey', 'options.publicKey'),
      'options.publicKey'));
  }
  const issuer = options.issuer;
  if (issuer !== undefined && !isX509Certificate(issuer)) {
    throw new ERR_INVALID_ARG_TYPE(
      'options.issuer', 'X509Certificate', issuer);
  }

  const serial = serialBytes(
    getRequired(options, 'serialNumber', 'options.serialNumber'));
  const notBefore = formatTime(
    getRequired(options, 'notBefore', 'options.notBefore'),
    'options.notBefore');
  const notAfterValue = getRequired(
    options, 'notAfter', 'options.notAfter');
  const notAfter = formatTime(notAfterValue, 'options.notAfter');
  if (notAfter.milliseconds <= notBefore.milliseconds) {
    throw new ERR_INVALID_ARG_VALUE(
      'options.notAfter', notAfterValue, 'must be after options.notBefore');
  }

  const signature = getRequired(options, 'signature', 'options.signature');
  validateObject(signature, 'options.signature');
  const signingKey = getRequired(
    signature, 'key', 'options.signature.key');
  const {
    data: signingKeyData,
    format: signingKeyFormat,
    type: signingKeyType,
    passphrase: signingKeyPassphrase,
    namedCurve: signingKeyNamedCurve,
  } = preparePrivateKey(signingKey, 'options.signature.key');
  const digest = getRequired(signature, 'digest', 'options.signature.digest');
  if (digest !== null) validateString(digest, 'options.signature.digest');
  const saltLength = getSaltLength(signingKey, 'options.signature.key');
  const padding = getPadding(signingKey, 'options.signature.key');
  let extensions;
  if (csr !== undefined) {
    const policy = getRequired(
      options, 'selectExtensions', 'options.selectExtensions');
    validateFunction(policy, 'options.selectExtensions');
    const { 0: preparedCsr, 1: requested } =
      parseX509CertificateRequest(csr);
    csr = preparedCsr;
    const requestedOptions = [];
    for (let i = 0; i < requested.length; i++) {
      const extension = requested[i];
      ArrayPrototypePush(requestedOptions, {
        type: 'raw',
        oid: extension.oid,
        critical: extension.critical,
        value: extension.value,
      });
    }
    const selected = FunctionPrototypeCall(policy, undefined, requestedOptions);
    validateArray(selected, 'options.selectExtensions return value');
    extensions = normalizeExtensions(
      selected, 'options.selectExtensions return value');
  } else {
    extensions = normalizeExtensions(options.extensions);
  }
  return new X509GenerateJob(
    mode,
    csr,
    subject,
    publicKeyData,
    publicKeyFormat,
    publicKeyType,
    publicKeyPassphrase,
    publicKeyNamedCurve,
    issuer,
    serial,
    notBefore.value,
    notAfter.value,
    extensions,
    signingKeyData,
    signingKeyFormat,
    signingKeyType,
    signingKeyPassphrase,
    signingKeyNamedCurve,
    digest,
    saltLength,
    padding,
  );
}

function generateX509Certificate(options, callback) {
  validateFunction(callback, 'callback');
  const job = normalizeOptions(options, kCryptoJobAsync);
  job.ondone = (error, der) => {
    if (error !== undefined)
      return FunctionPrototypeCall(callback, job, error);
    let certificate;
    try {
      certificate = new X509Certificate(Buffer.from(der));
    } catch (err) {
      return FunctionPrototypeCall(callback, job, err);
    }
    return FunctionPrototypeCall(callback, job, null, certificate);
  };
  job.run();
}

function generateX509CertificateSync(options) {
  const job = normalizeOptions(options, kCryptoJobSync);
  const { 0: error, 1: der } = job.run();
  if (error !== undefined) throw error;
  return new X509Certificate(Buffer.from(der));
}

module.exports = {
  generateX509Certificate,
  generateX509CertificateSync,
};
