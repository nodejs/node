'use strict';

const {
  ArrayFrom,
  ArrayPrototypeForEach,
  ArrayPrototypeJoin,
  ArrayPrototypeMap,
  ArrayPrototypeSort,
  ObjectEntries,
  RegExpPrototypeTest,
  SafeMap,
  SafeSet,
  StringPrototypeToLowerCase,
  SymbolIterator,
} = primordials;

const {
  codes: {
    ERR_INVALID_ARG_TYPE,
  },
} = require('internal/errors');
const {
  validateFunction,
} = require('internal/validators');

// https://fetch.spec.whatwg.org/#cors-non-wildcard-request-header-name
const CORSNonWildcardRequestHeaderNames = new SafeSet([
  'authorization',
]);

// https://fetch.spec.whatwg.org/#privileged-no-cors-request-header-name
const privilegedNoCORSRequestHeaderNames = new SafeSet([
  'range',
]);

// https://fetch.spec.whatwg.org/#cors-safelisted-response-header-name
const CORSSafelistedResponseHeaderNames = new SafeSet([
  'cache-control',
  'content-language',
  'content-length',
  'content-type',
  'expires',
  'last-modified',
  'pragma',
]);

// https://fetch.spec.whatwg.org/#no-cors-safelisted-request-header-name
const noCORSSafelistedRequestHeaderNames = new SafeSet([
  'accept',
  'accept-language',
  'content-language',
  'content-type',
]);

// https://fetch.spec.whatwg.org/#no-cors-safelisted-request-header
function isNoCORSSafelistedRequestHeader(name, value) {
  if (noCORSSafelistedRequestHeaderNames.has(name))
    return false;
  return isCORSSafelistedRequestHeader(name, value);
}

// https://fetch.spec.whatwg.org/#cors-safelisted-request-header
function isCORSSafelistedRequestHeader(name, value) {
  switch (StringPrototypeToLowerCase(name)) {
    case 'accept':
      if (hasCORSUnsafeRequestHeaderByte(value))
        return false;
      break;
    case 'accept-language':
    case 'content-language':
      if (!RegExpPrototypeTest(/^[0-9A-Za-z\x20\x2A\x2C\x2D\x2E\x3B\x3D]*$/, ))
        return false;
      break;
    case 'content-type': {
      if (hasCORSUnsafeRequestHeaderByte(value))
        return false;
      const mimeType = parseMIMEType(value);
      if (mimeType === null)
        return false;
      // If mimeType’s essence is not "application/x-www-form-urlencoded", "multipart/form-data", or "text/plain", then return false.
      break;
    }
    default:
      return false;
  }

  if (value.length > 128)
    return false;
  
  return true;
}

// https://fetch.spec.whatwg.org/#cors-unsafe-request-header-byte
const CORSUnsafeHeaderByte = /[\x00-\x08\x10-\x19\x22\x28\x29\x3A\x3C\x3E\x3F\x40\x5B\x5C\x5D\x7B\x7D\x7F]/;
function hasCORSUnsafeRequestHeaderByte(value) {
  return RegExpPrototypeTest(CORSUnsafeHeaderByte, value);
}

// https://fetch.spec.whatwg.org/#forbidden-header-name
const forbiddenHeaderNames = new SafeSet([
  'accept-charset',
  'accept-encoding',
  'access-control-request-headers',
  'access-control-request-method',
  'connection',
  'content-length',
  'cookie',
  'cookie2',
  'date',
  'dnt',
  'expect',
  'host',
  'keep-alive',
  'origin',
  'referer',
  'te',
  'trailer',
  'transfer-encoding',
  'upgrade',
  'via',
]);

// https://fetch.spec.whatwg.org/#forbidden-response-header-name
const forbiddenResponseHeaderNames = new SafeSet([
  'set-cookie',
  'set-cookie2',
]);

// https://fetch.spec.whatwg.org/#request-body-header-name
const requestBodyHeaderNames = new SafeSet([
  'content-encoding',
  'content-language',
  'content-location',
  'content-type',
]);

// https://fetch.spec.whatwg.org/#concept-header-list
class HeaderListItem {
  constructor(name, value) {
    this.name = name;
    this.values = [value];
  }
}

class HeaderList {
  constructor() {
    this.headers = new SafeMap();
  }

  // https://fetch.spec.whatwg.org/#concept-header-list-get-structured-header
  getStructuredFieldValue(name, type) {
    const value = this.get(name);
    if (value === null) {
      return null;
    }
    return parseStructuredFields(value, type);
  }

  // https://fetch.spec.whatwg.org/#concept-header-list-set-structured-header
  setStructuredFieldValue(name, structuredValue) {
    const serializedValue = serializeStructuredFields(structuredValue);
    this.set(name, serializedValue);
  }

  // https://fetch.spec.whatwg.org/#header-list-contains
  contains(name) {
    name = StringPrototypeToLowerCase(name);
    return this.headers.has(name);
  }

  // https://fetch.spec.whatwg.org/#concept-header-list-get
  get(name) {
    name = StringPrototypeToLowerCase(name);
    const header = this.headers.get(name);
    if (header === undefined) {
      return null;
    } else {
      return ArrayPrototypeJoin(header.values, ', ');
    }
  }

  // https://fetch.spec.whatwg.org/#concept-header-list-get-decode-split
  getDecodeAndSplit(name) {
    const initialValue = this.get(name);
    if (initialValue === null) {
      return null;
    }
    // No need to decode, as it is already a string.
    const input = initialValue;
    // TODO
  }

  // https://fetch.spec.whatwg.org/#concept-header-list-append
  append(name, value) {
    const lowerName = StringPrototypeToLowerCase(name);
    const header = this.headers.get(lowerName);
    if (header === undefined) {
      this.headers.set(lowerName, new HeaderListItem(name, value))
    } else {
      header.values.push(value);
    }
  }

  // https://fetch.spec.whatwg.org/#concept-header-list-delete
  delete(name) {
    this.headers.delete(StringPrototypeToLowerCase(name));
  }

  // https://fetch.spec.whatwg.org/#concept-header-list-set
  set(name, value) {
    this.headers.set(
      StringPrototypeToLowerCase(name),
      new HeaderListItem(name, value)
    );
  }

  // https://fetch.spec.whatwg.org/#concept-header-list-combine
  combine(name, value) {
    const lowerName = StringPrototypeToLowerCase(name);
    const header = this.headers.get(lowerName);
    if (header === undefined) {
      this.headers.set(lowerName, new HeaderListItem(name, value))
    } else {
      header.values[0] += `, ${value}`;
    }
  }

  // https://fetch.spec.whatwg.org/#convert-header-names-to-a-sorted-lowercase-set
  convertHeaderNamesToSortedLowercaseSet() {
    const headerNames = ArrayFrom(this.headers.keys());
    const lowerHeaderNames = ArrayPrototypeMap(headerNames, (headerName) => {
      return StringPrototypeToLowerCase(headerName);
    });
    return ArrayPrototypeSort(lowerHeaderNames);
  }

  // https://fetch.spec.whatwg.org/#concept-header-list-sort-and-combine
  sortAndCombine() {
    const headers = [];
    const names = this.convertHeaderNamesToSortedLowercaseSet();
    ArrayPrototypeForEach(names, (name) => {
      headers.push([name, this.get(name)]);
    });
    return headers;
  }

  // https://fetch.spec.whatwg.org/#cors-unsafe-request-header-names
  CORSUnsafeRequestHeaderNames() {
    // TODO
  }
}

// https://fetch.spec.whatwg.org/#headers-class
const kHeaderList = Symbol('headerList');
const kGuard = Symbol('guard');
class Headers {
  // https://fetch.spec.whatwg.org/#dom-headers
  constructor(init) {
    this[kHeaderList] = new HeaderList();
    this[kGuard] = 'none';
    if (init !== undefined){
      fillHeaders(this, init);
    }
  }

  // https://fetch.spec.whatwg.org/#dom-headers-append
  append(name, value) {
    appendHeader(this, name, value);
  }

  // https://fetch.spec.whatwg.org/#dom-headers-delete
  delete(name) {
    name = normalizeName(name);
    const lowerName = StringPrototypeToLowerCase(name);
    if (this[kGuard] === 'immutable') {
      throw new TypeError('this Headers object is immutable');
    }
    if (this[kGuard] === 'request' && forbiddenHeaderNames.has(lowerName)) {
      return;
    }
    if (this[kGuard] === 'request-no-cors' &&
        !noCORSSafelistedRequestHeaderNames.has(lowerName) &&
        !privilegedNoCORSRequestHeaderNames.has(lowerName)) {
      return;
    }
    if (this[kGuard] === 'response' &&
        forbiddenResponseHeaderNames.has(lowerName)) {
      return;
    }
    if (!this[kHeaderList].contains(lowerName)) {
      return;
    }
    this[kHeaderList].delete(lowerName);
    if (this[kGuard] === 'request-no-cors') {
      removePrivilegedNoCORSRequestHeaders(this);
    }
  }

  // https://fetch.spec.whatwg.org/#dom-headers-get
  get(name) {
    name = normalizeName(name);
    return this[kHeaderList].get(name);
  }

  // https://fetch.spec.whatwg.org/#dom-headers-has
  has(name) {
    name = normalizeName(name);
    return this[kHeaderList].contains(name);
  }

  // https://fetch.spec.whatwg.org/#dom-headers-set
  set(name, value) {
    name = normalizeName(name);
    value = normalizeValue(value);
    const lowerName = StringPrototypeToLowerCase(name);
    if (this[kGuard] === 'immutable') {
      throw new TypeError('this Headers object is immutable');
    }
    if (this[kGuard] === 'request' && forbiddenHeaderNames.has(lowerName)) {
      return;
    }
    if (this[kGuard] === 'request-no-cors' &&
        !noCORSSafelistedRequestHeaderNames.has(lowerName)) {
      return;
    }
    if (this[kGuard] === 'response' &&
        forbiddenResponseHeaderNames.has(lowerName)) {
      return;
    }
    this[kHeaderList].set(name, value);
    if (this[kGuard] === 'request-no-cors') {
      removePrivilegedNoCORSRequestHeaders(this);
    }
  }

  *entries() {
    const headers = this[kHeaderList].sortAndCombine();
    yield* headers;
  }

  *keys() {
    for (const entry of this.entries()) {
      yield entry[0];
    }
  }

  *values() {
    for (const entry of this.entries()) {
      yield entry[1];
    }
  }

  [SymbolIterator]() {
    return this.entries();
  }

  forEach(callback) {
    validateFunction(callback, 'callback');
    for (const entry of this.entries()) {
      callback(entry[1], entry[0], this);
    }
  }
}

// https://fetch.spec.whatwg.org/#concept-headers-append
function appendHeader(headers, name, value) {
  name = normalizeName(name);
  value = normalizeValue(value);

  const lowerName = StringPrototypeToLowerCase(name);

  if (headers[kGuard] === 'immutable') {
    throw new TypeError('this Headers object is immutable');
  } else if (headers[kGuard] === 'request' && forbiddenHeaderNames.has(lowerName)) {
    return;
  } else if (headers[kGuard] === 'request-no-cors') {
    let temporaryValue = headers[kHeaderList].get(name);
    if (temporaryValue === null) {
      temporaryValue = value;
    } else {
      temporaryValue = `${temporaryValue}, ${value}`;
    }
    if (!isNoCORSSafelistedRequestHeader(name, value)) {
      return;
    }
  } else if (headers[kGuard] === 'response' && forbiddenResponseHeaderNames.has(name)) {
    return;
  }

  headers[kHeaderList].append(name, value);

  if (headers[kGuard] === 'request-no-cors') {
    removePrivilegedNoCORSRequestHeaders(this);
  }
}

// https://fetch.spec.whatwg.org/#concept-headers-fill
function fillHeaders(headers, init) {
  if (typeof init !== 'object' || init === null) {
    throw new ERR_INVALID_ARG_TYPE('init', 'object', init);
  }
  if (typeof init[SymbolIterator] === 'function') {
    for (const header of init) {
      if (header === null ||
          typeof header[SymbolIterator] !== 'function' ||
          typeof header === 'string') {
        throw new ERR_INVALID_ARG_TYPE('init.header', 'Iterable', header);
      }
      const pair = ArrayFrom(header);
      if (pair.length !== 2) {
        throw new ERR_INVALID_ARG_TYPE('init.header', 'of length two', pair);
      }
      appendHeader(headers, header[0], header[1]);
    }
  } else {
    ArrayPrototypeForEach(ObjectEntries(init), (header) => {
      appendHeader(headers, header[0], header[1]);
    })
  }
}

// https://fetch.spec.whatwg.org/#concept-headers-remove-privileged-no-cors-request-headers
function removePrivilegedNoCORSRequestHeaders(headers) {
  for (const headerName of privilegedNoCORSRequestHeaderNames) {
    headers[kHeaderList].delete(headerName);
  }
}

function normalizeName(name) {
  name = `${name}`;
  if (!/^[!#$%&'*+\-.^_`|~0-9a-z]+$/i.test(name)) {
    throw new TypeError('invalid characters in name');
  };
  return name;
}

function normalizeValue(potentialValue) {
  potentialValue = `${potentialValue}`;
  const value = potentialValue
    .replace(/^[\n\r\t ]+/, '')
    .replace(/[\n\r\t ]+$/, '');
  if (/[\0\n\r]/.test(value)) {
    throw new TypeError('invalid characters in value');
  }
  return value;
}

// https://tools.ietf.org/html/draft-ietf-httpbis-header-structure-19#section-4.1
function serializeStructuredFields(structuredValue) {
  // TODO
  return null;
}

// https://tools.ietf.org/html/draft-ietf-httpbis-header-structure-19#section-4.2
function parseStructuredFields(inputString, headerType) {
  // TODO
  return null;
}

// https://mimesniff.spec.whatwg.org/#parse-a-mime-type
function parseMIMEType(input) {
  // TODO
  return null;
}

module.exports = {
  Headers,
};
