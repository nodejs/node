'use strict';

const {
  FunctionPrototypeCall,
  ObjectGetPrototypeOf,
  ObjectSetPrototypeOf,
  SafeWeakMap,
} = primordials;

const {
  createX509CertificateClass,
  isX509Certificate: isNativeX509Certificate,
  X509_CHECK_FLAG_ALWAYS_CHECK_SUBJECT,
  X509_CHECK_FLAG_NEVER_CHECK_SUBJECT,
  X509_CHECK_FLAG_NO_WILDCARDS,
  X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS,
  X509_CHECK_FLAG_MULTI_LABEL_WILDCARDS,
  X509_CHECK_FLAG_SINGLE_LABEL_SUBDOMAINS,
} = internalBinding('crypto');

const {
  PublicKeyObject,
  getKeyObjectHandle,
  getKeyObjectType,
  isKeyObject,
} = require('internal/crypto/keys');

const {
  customInspectSymbol: kInspect,
  kEmptyObject,
} = require('internal/util');

const {
  validateBoolean,
  validateObject,
  validateString,
} = require('internal/validators');

const { inspect } = require('internal/util/inspect');

const { Buffer } = require('buffer');

const {
  isArrayBufferView,
} = require('internal/util/types');

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_THIS,
  },
} = require('internal/errors');

let lazyTranslatePeerCertificate;

let isX509Certificate;

function getFlags(options = kEmptyObject) {
  validateObject(options, 'options');
  const {
    subject = 'default',  // Can be 'default', 'always', or 'never'
    wildcards = true,
    partialWildcards = true,
    multiLabelWildcards = false,
    singleLabelSubdomains = false,
  } = { ...options };
  let flags = 0;
  validateString(subject, 'options.subject');
  validateBoolean(wildcards, 'options.wildcards');
  validateBoolean(partialWildcards, 'options.partialWildcards');
  validateBoolean(multiLabelWildcards, 'options.multiLabelWildcards');
  validateBoolean(singleLabelSubdomains, 'options.singleLabelSubdomains');
  switch (subject) {
    case 'default': /* Matches OpenSSL's default, no flags. */ break;
    case 'always': flags |= X509_CHECK_FLAG_ALWAYS_CHECK_SUBJECT; break;
    case 'never': flags |= X509_CHECK_FLAG_NEVER_CHECK_SUBJECT; break;
    default:
      throw new ERR_INVALID_ARG_VALUE('options.subject', subject);
  }
  if (!wildcards) flags |= X509_CHECK_FLAG_NO_WILDCARDS;
  if (!partialWildcards) flags |= X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS;
  if (multiLabelWildcards) flags |= X509_CHECK_FLAG_MULTI_LABEL_WILDCARDS;
  if (singleLabelSubdomains) flags |= X509_CHECK_FLAG_SINGLE_LABEL_SUBDOMAINS;
  return flags;
}

const crossRealmState = new SafeWeakMap();

const kSlotSubject = 0;
const kSlotSubjectAltName = 1;
const kSlotIssuer = 2;
const kSlotIssuerCertificate = 3;
const kSlotInfoAccess = 4;
const kSlotValidFrom = 5;
const kSlotValidTo = 6;
const kSlotValidFromDate = 7;
const kSlotValidToDate = 8;
const kSlotFingerprint = 9;
const kSlotFingerprint256 = 10;
const kSlotFingerprint512 = 11;
const kSlotKeyUsage = 12;
const kSlotSerialNumber = 13;
const kSlotSignatureAlgorithm = 14;
const kSlotSignatureAlgorithmOid = 15;
const kSlotRaw = 16;
const kSlotPublicKey = 17;
const kSlotPem = 18;
const kSlotCA = 19;
const kStateCacheMask = 20;

function createState() {
  const state = [];
  state[kStateCacheMask] = 0;
  return state;
}

let InternalX509Certificate;

const {
  0: X509Certificate,
  1: InternalX509CertificateConstructor,
} = createX509CertificateClass((NativeX509Certificate) => {
  const {
    checkCA: nativeCheckCA,
    checkEmail: nativeCheckEmail,
    checkHost: nativeCheckHost,
    checkIP: nativeCheckIP,
    checkIssued: nativeCheckIssued,
    checkPrivateKey: nativeCheckPrivateKey,
    fingerprint: nativeFingerprint,
    fingerprint256: nativeFingerprint256,
    fingerprint512: nativeFingerprint512,
    getIssuerCert: nativeGetIssuerCert,
    infoAccess: nativeInfoAccess,
    issuer: nativeIssuer,
    keyUsage: nativeKeyUsage,
    pem: nativePem,
    publicKey: nativePublicKey,
    raw: nativeRaw,
    serialNumber: nativeSerialNumber,
    signatureAlgorithm: nativeSignatureAlgorithm,
    signatureAlgorithmOid: nativeSignatureAlgorithmOid,
    subject: nativeSubject,
    subjectAltName: nativeSubjectAltName,
    toLegacy: nativeToLegacy,
    validFrom: nativeValidFrom,
    validFromDate: nativeValidFromDate,
    validTo: nativeValidTo,
    validToDate: nativeValidToDate,
    verify: nativeVerify,
  } = NativeX509Certificate.prototype;

  let getState;

  function getCachedValue(cert, index, getter) {
    const state = getState(cert);
    const bit = 1 << index;
    if ((state[kStateCacheMask] & bit) === 0) {
      state[index] = FunctionPrototypeCall(getter, cert);
      state[kStateCacheMask] |= bit;
    }
    return state[index];
  }

  class X509Certificate {
    constructor(buffer) {
      if (typeof buffer === 'string')
        buffer = Buffer.from(buffer);
      if (!isArrayBufferView(buffer)) {
        throw new ERR_INVALID_ARG_TYPE(
          'buffer',
          ['string', 'Buffer', 'TypedArray', 'DataView'],
          buffer);
      }
      const prototype = ObjectGetPrototypeOf(this);
      const certificate = new InternalX509Certificate(buffer);
      ObjectSetPrototypeOf(certificate, prototype);
      // eslint-disable-next-line no-constructor-return
      return certificate;
    }

    [kInspect](depth, options) {
      if (depth < 0)
        return this;

      const opts = {
        ...options,
        depth: options.depth == null ? null : options.depth - 1,
      };

      return `X509Certificate ${inspect({
        subject: getCachedValue(this, kSlotSubject, nativeSubject),
        subjectAltName:
          getCachedValue(this, kSlotSubjectAltName, nativeSubjectAltName),
        issuer: getCachedValue(this, kSlotIssuer, nativeIssuer),
        infoAccess: getCachedValue(this, kSlotInfoAccess, nativeInfoAccess),
        validFrom: getCachedValue(this, kSlotValidFrom, nativeValidFrom),
        validTo: getCachedValue(this, kSlotValidTo, nativeValidTo),
        validFromDate:
          getCachedValue(this, kSlotValidFromDate, nativeValidFromDate),
        validToDate:
          getCachedValue(this, kSlotValidToDate, nativeValidToDate),
        fingerprint:
          getCachedValue(this, kSlotFingerprint, nativeFingerprint),
        fingerprint256:
          getCachedValue(this, kSlotFingerprint256, nativeFingerprint256),
        fingerprint512:
          getCachedValue(this, kSlotFingerprint512, nativeFingerprint512),
        keyUsage: getCachedValue(this, kSlotKeyUsage, nativeKeyUsage),
        serialNumber:
          getCachedValue(this, kSlotSerialNumber, nativeSerialNumber),
        signatureAlgorithm:
          getCachedValue(
            this, kSlotSignatureAlgorithm, nativeSignatureAlgorithm),
        signatureAlgorithmOid:
          getCachedValue(
            this, kSlotSignatureAlgorithmOid, nativeSignatureAlgorithmOid),
      }, opts)}`;
    }

    get subject() {
      return getCachedValue(this, kSlotSubject, nativeSubject);
    }

    get subjectAltName() {
      return getCachedValue(
        this, kSlotSubjectAltName, nativeSubjectAltName);
    }

    get issuer() {
      return getCachedValue(this, kSlotIssuer, nativeIssuer);
    }

    get issuerCertificate() {
      const state = getState(this);
      const bit = 1 << kSlotIssuerCertificate;
      if ((state[kStateCacheMask] & bit) === 0) {
        const cert = FunctionPrototypeCall(nativeGetIssuerCert, this);
        state[kSlotIssuerCertificate] = cert ?
          new InternalX509Certificate(cert) : undefined;
        state[kStateCacheMask] |= bit;
      }
      return state[kSlotIssuerCertificate];
    }

    get infoAccess() {
      return getCachedValue(this, kSlotInfoAccess, nativeInfoAccess);
    }

    get validFrom() {
      return getCachedValue(this, kSlotValidFrom, nativeValidFrom);
    }

    get validTo() {
      return getCachedValue(this, kSlotValidTo, nativeValidTo);
    }

    get validFromDate() {
      return getCachedValue(this, kSlotValidFromDate, nativeValidFromDate);
    }

    get validToDate() {
      return getCachedValue(this, kSlotValidToDate, nativeValidToDate);
    }

    get fingerprint() {
      return getCachedValue(this, kSlotFingerprint, nativeFingerprint);
    }

    get fingerprint256() {
      return getCachedValue(this, kSlotFingerprint256, nativeFingerprint256);
    }

    get fingerprint512() {
      return getCachedValue(this, kSlotFingerprint512, nativeFingerprint512);
    }

    get keyUsage() {
      return getCachedValue(this, kSlotKeyUsage, nativeKeyUsage);
    }

    get serialNumber() {
      return getCachedValue(this, kSlotSerialNumber, nativeSerialNumber);
    }

    get signatureAlgorithm() {
      return getCachedValue(
        this, kSlotSignatureAlgorithm, nativeSignatureAlgorithm);
    }

    get signatureAlgorithmOid() {
      return getCachedValue(
        this, kSlotSignatureAlgorithmOid, nativeSignatureAlgorithmOid);
    }

    get raw() {
      return getCachedValue(this, kSlotRaw, nativeRaw);
    }

    get publicKey() {
      const state = getState(this);
      const bit = 1 << kSlotPublicKey;
      if ((state[kStateCacheMask] & bit) === 0) {
        state[kSlotPublicKey] = new PublicKeyObject(
          FunctionPrototypeCall(nativePublicKey, this));
        state[kStateCacheMask] |= bit;
      }
      return state[kSlotPublicKey];
    }

    toString() {
      return getCachedValue(this, kSlotPem, nativePem);
    }

    // There's no standardized JSON encoding for X509 certs so we
    // fallback to providing the PEM encoding as a string.
    toJSON() { return getCachedValue(this, kSlotPem, nativePem); }

    get ca() {
      return getCachedValue(this, kSlotCA, nativeCheckCA);
    }

    checkHost(name, options) {
      getState(this);
      validateString(name, 'name');
      return FunctionPrototypeCall(
        nativeCheckHost, this, name, getFlags(options));
    }

    checkEmail(email, options) {
      getState(this);
      validateString(email, 'email');
      return FunctionPrototypeCall(
        nativeCheckEmail, this, email, getFlags(options));
    }

    checkIP(ip, options) {
      getState(this);
      validateString(ip, 'ip');
      // The options argument is currently undocumented since none of the
      // options have any effect on the behavior of this function. However, we
      // still parse the options argument in case OpenSSL adds flags in the
      // future that do affect the behavior of X509_check_ip. This ensures that
      // no invalid values are passed as the second argument in the meantime.
      return FunctionPrototypeCall(
        nativeCheckIP, this, ip, getFlags(options));
    }

    checkIssued(otherCert) {
      getState(this);
      if (!isX509Certificate(otherCert)) {
        throw new ERR_INVALID_ARG_TYPE(
          'otherCert', 'X509Certificate', otherCert);
      }
      return FunctionPrototypeCall(nativeCheckIssued, this, otherCert);
    }

    checkPrivateKey(pkey) {
      getState(this);
      if (!isKeyObject(pkey))
        throw new ERR_INVALID_ARG_TYPE('pkey', 'KeyObject', pkey);
      if (getKeyObjectType(pkey) !== 'private')
        throw new ERR_INVALID_ARG_VALUE('pkey', pkey);
      return FunctionPrototypeCall(
        nativeCheckPrivateKey, this, getKeyObjectHandle(pkey));
    }

    verify(pkey) {
      getState(this);
      if (!isKeyObject(pkey))
        throw new ERR_INVALID_ARG_TYPE('pkey', 'KeyObject', pkey);
      if (getKeyObjectType(pkey) !== 'public')
        throw new ERR_INVALID_ARG_VALUE('pkey', pkey);
      return FunctionPrototypeCall(
        nativeVerify, this, getKeyObjectHandle(pkey));
    }

    toLegacyObject() {
      getState(this);
      // TODO(tniessen): do not depend on translatePeerCertificate here, return
      // the correct legacy representation from the binding
      lazyTranslatePeerCertificate ??=
        require('internal/tls/common').translatePeerCertificate;
      return lazyTranslatePeerCertificate(
        FunctionPrototypeCall(nativeToLegacy, this));
    }
  }

  InternalX509Certificate = class InternalX509Certificate
    extends NativeX509Certificate {
    #state = createState();

    static {
      isX509Certificate = (value) => {
        if (value == null || typeof value !== 'object') return false;
        return #state in value || isNativeX509Certificate(value);
      };

      getState = (cert) => {
        try {
          return cert.#state;
        } catch {
          // Continue with the cross-realm native brand check.
        }
        if (!isNativeX509Certificate(cert)) {
          throw new ERR_INVALID_THIS('X509Certificate');
        }
        let state = crossRealmState.get(cert);
        if (state === undefined) {
          state = createState();
          crossRealmState.set(cert, state);
        }
        return state;
      };
    }
  };

  InternalX509Certificate.prototype.constructor = X509Certificate;
  ObjectSetPrototypeOf(
    InternalX509Certificate.prototype,
    X509Certificate.prototype);

  return [X509Certificate, InternalX509Certificate];
});

// Keep the binding-returned constructor and the closure reference aligned.
InternalX509Certificate = InternalX509CertificateConstructor;

module.exports = {
  X509Certificate,
  InternalX509Certificate,
  isX509Certificate,
};
