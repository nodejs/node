'use strict';

// The ServerHeaders protocol of the Fetch Server API.
// https://fetch-server.proposal.wintertc.org/#serverheaders

const {
  ArrayIsArray,
  ArrayPrototypeJoin,
  ArrayPrototypePush,
  ArrayPrototypeSort,
  ArrayPrototypeSplice,
  FunctionPrototypeCall,
  ObjectDefineProperties,
  ObjectGetOwnPropertyDescriptor,
  ObjectGetPrototypeOf,
  ObjectSetPrototypeOf,
  ReflectGet,
  ReflectOwnKeys,
  RegExpPrototypeExec,
  RegExpPrototypeSymbolReplace,
  SafeSet,
  StringPrototypeCharCodeAt,
  StringPrototypeToLowerCase,
  Symbol,
  SymbolIterator,
  SymbolToStringTag,
} = primordials;

const {
  codes: {
    ERR_HTTP_HEADERS_SENT,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_HTTP_TOKEN,
    ERR_INVALID_THIS,
  },
} = require('internal/errors');

const {
  _checkIsHttpToken: checkIsHttpToken,
} = require('internal/http/common');

const {
  converters,
} = require('internal/webidl');

const {
  customInspectSymbol: kInspect,
  kEnumerableProperty,
} = require('internal/util');

const { inspect } = require('internal/util/inspect');

// eslint-disable-next-line no-control-regex
const kInvalidValueRegExp = /[\x00\x0a\x0d]/;
const kLeadingTrailingWhitespace = /^[\t\n\r ]+|[\t\n\r ]+$/g;

/**
 * Converts V to a Web IDL ByteString.
 * @param {any} V
 * @param {string} name
 * @returns {string}
 */
function toByteString(V, name) {
  const str = converters.DOMString(V, { __proto__: null, prefix: name });
  for (let i = 0; i < str.length; i++) {
    if (StringPrototypeCharCodeAt(str, i) > 0xff) {
      throw new ERR_INVALID_ARG_VALUE(name, str,
                                      'must only contain characters <= U+00FF');
    }
  }
  return str;
}

function normalizeValue(value) {
  return RegExpPrototypeSymbolReplace(kLeadingTrailingWhitespace, value, '');
}

function isHeaderName(name) {
  return name.length > 0 && checkIsHttpToken(name);
}

function isHeaderValue(value) {
  if (value.length === 0) return true;
  const first = StringPrototypeCharCodeAt(value, 0);
  const last = StringPrototypeCharCodeAt(value, value.length - 1);
  if (first === 0x20 || first === 0x09 || last === 0x20 || last === 0x09) {
    return false;
  }
  return RegExpPrototypeExec(kInvalidValueRegExp, value) === null;
}

function validateName(name) {
  if (!isHeaderName(name)) {
    throw new ERR_INVALID_HTTP_TOKEN('Header name', name);
  }
}

function validateValue(name, value) {
  if (!isHeaderValue(value)) {
    throw new ERR_INVALID_ARG_VALUE('value', value,
                                    `is not a valid value for header "${name}"`);
  }
}

// A prohibited trailer name (Section 10.3).
const kProhibitedTrailers = new SafeSet([
  'content-length', 'transfer-encoding', 'trailer',
  'connection', 'keep-alive', 'proxy-connection', 'te', 'upgrade',
  'host',
  'authorization', 'proxy-authorization', 'www-authenticate',
  'proxy-authenticate', 'cookie', 'set-cookie',
  'cache-control', 'expect', 'max-forwards', 'pragma', 'range', 'if-match',
  'if-none-match', 'if-modified-since', 'if-unmodified-since', 'if-range',
  'age', 'expires', 'date', 'location', 'retry-after', 'vary',
  'content-encoding', 'content-type', 'content-range',
]);

function isProhibitedTrailerName(name) {
  return kProhibitedTrailers.has(StringPrototypeToLowerCase(name));
}

/**
 * Removes fields with a prohibited trailer name from a header list.
 * @param {Array<[string, string]>} list
 * @returns {Array<[string, string]>}
 */
function filterTrailers(list) {
  const result = [];
  for (let i = 0; i < list.length; i++) {
    if (!isProhibitedTrailerName(list[i][0])) {
      ArrayPrototypePush(result, list[i]);
    }
  }
  return result;
}

// =============================================================================
// Header list operations (Fetch Standard)
// =============================================================================
// A header list is an Array of [name, value] pairs.

function listIndexOf(list, lowerName, from = 0) {
  for (let i = from; i < list.length; i++) {
    if (StringPrototypeToLowerCase(list[i][0]) === lowerName) return i;
  }
  return -1;
}

function listContains(list, name) {
  return listIndexOf(list, StringPrototypeToLowerCase(name)) !== -1;
}

function listGet(list, name) {
  const lower = StringPrototypeToLowerCase(name);
  let values;
  for (let i = 0; i < list.length; i++) {
    if (StringPrototypeToLowerCase(list[i][0]) === lower) {
      values ??= [];
      ArrayPrototypePush(values, list[i][1]);
    }
  }
  return values === undefined ? null : ArrayPrototypeJoin(values, ', ');
}

function listGetAll(list, name) {
  const lower = StringPrototypeToLowerCase(name);
  const values = [];
  for (let i = 0; i < list.length; i++) {
    if (StringPrototypeToLowerCase(list[i][0]) === lower) {
      ArrayPrototypePush(values, list[i][1]);
    }
  }
  return values;
}

function listDelete(list, name) {
  const lower = StringPrototypeToLowerCase(name);
  for (let i = list.length - 1; i >= 0; i--) {
    if (StringPrototypeToLowerCase(list[i][0]) === lower) {
      ArrayPrototypeSplice(list, i, 1);
    }
  }
}

function listSet(list, name, value) {
  const lower = StringPrototypeToLowerCase(name);
  const index = listIndexOf(list, lower);
  if (index === -1) {
    ArrayPrototypePush(list, [name, value]);
    return;
  }
  list[index] = [list[index][0], value];
  for (let i = list.length - 1; i > index; i--) {
    if (StringPrototypeToLowerCase(list[i][0]) === lower) {
      ArrayPrototypeSplice(list, i, 1);
    }
  }
}

function compareNames(a, b) {
  return a[0] < b[0] ? -1 : a[0] > b[0] ? 1 : 0;
}

function sortAndCombine(list) {
  const names = [];
  const seen = { __proto__: null };
  for (let i = 0; i < list.length; i++) {
    const lower = StringPrototypeToLowerCase(list[i][0]);
    if (seen[lower] === undefined) {
      seen[lower] = true;
      ArrayPrototypePush(names, [lower]);
    }
  }
  ArrayPrototypeSort(names, compareNames);
  const result = [];
  for (let i = 0; i < names.length; i++) {
    const name = names[i][0];
    if (name === 'set-cookie') {
      const values = listGetAll(list, name);
      for (let j = 0; j < values.length; j++) {
        ArrayPrototypePush(result, [name, values[j]]);
      }
    } else {
      ArrayPrototypePush(result, [name, listGet(list, name)]);
    }
  }
  return result;
}

function copyList(list) {
  const copy = [];
  for (let i = 0; i < list.length; i++) {
    ArrayPrototypePush(copy, [list[i][0], list[i][1]]);
  }
  return copy;
}

/**
 * Converts a ServerHeadersInit to a header list (Section 10.1).
 * @param {any} object
 * @param {string} [argName]
 * @returns {Array<[string, string]>}
 */
function convertHeadersInit(object, argName = 'headers') {
  if (object === null ||
      (typeof object !== 'object' && typeof object !== 'function')) {
    throw new ERR_INVALID_ARG_TYPE(argName, ['Array', 'Object'], object);
  }
  const list = [];
  const iteratorMethod = object[SymbolIterator];
  if (iteratorMethod !== undefined && iteratorMethod !== null) {
    if (typeof iteratorMethod !== 'function') {
      throw new ERR_INVALID_ARG_TYPE(argName, ['Array', 'Object'], object);
    }
    for (const pair of { [SymbolIterator]: () => FunctionPrototypeCall(iteratorMethod, object) }) {
      if (pair === null ||
          (typeof pair !== 'object' && typeof pair !== 'function') ||
          typeof pair[SymbolIterator] !== 'function') {
        throw new ERR_INVALID_ARG_VALUE(argName, pair,
                                        'must contain name/value pairs');
      }
      const items = [];
      for (const item of pair) {
        ArrayPrototypePush(items, toByteString(item, argName));
      }
      if (items.length !== 2) {
        throw new ERR_INVALID_ARG_VALUE(argName, pair,
                                        'must contain name/value pairs');
      }
      const name = items[0];
      const value = normalizeValue(items[1]);
      validateName(name);
      validateValue(name, value);
      ArrayPrototypePush(list, [name, value]);
    }
    return list;
  }
  const keys = ReflectOwnKeys(object);
  for (let i = 0; i < keys.length; i++) {
    const key = keys[i];
    const desc = ObjectGetOwnPropertyDescriptor(object, key);
    if (desc === undefined || !desc.enumerable) continue;
    const name = toByteString(key, argName);
    const value = normalizeValue(toByteString(ReflectGet(object, key), argName));
    validateName(name);
    validateValue(name, value);
    ArrayPrototypePush(list, [name, value]);
  }
  return list;
}

// =============================================================================
// ServerHeaders
// =============================================================================

const kConstruct = Symbol('kConstruct');

let getHeaderList;
let setHeadersImmutable;
let isServerHeaders;

class ServerHeaders {
  #list;
  #immutable;

  static {
    getHeaderList = (headers) => headers.#list;
    setHeadersImmutable = (headers) => { headers.#immutable = true; };
    isServerHeaders = (value) => {
      return value != null && typeof value === 'object' && #list in value;
    };
  }

  constructor(key, list, immutable) {
    if (key !== kConstruct) {
      throw new ERR_INVALID_THIS('ServerHeaders cannot be constructed');
    }
    this.#list = list;
    this.#immutable = immutable;
  }

  #validate(name, value) {
    validateName(name);
    validateValue(name, value);
    if (this.#immutable) {
      throw new ERR_HTTP_HEADERS_SENT('modify');
    }
  }

  append(name, value) {
    if (!isServerHeaders(this)) throw new ERR_INVALID_THIS('ServerHeaders');
    name = toByteString(name, 'name');
    value = normalizeValue(toByteString(value, 'value'));
    this.#validate(name, value);
    ArrayPrototypePush(this.#list, [name, value]);
  }

  delete(name) {
    if (!isServerHeaders(this)) throw new ERR_INVALID_THIS('ServerHeaders');
    name = toByteString(name, 'name');
    this.#validate(name, '');
    listDelete(this.#list, name);
  }

  get(name) {
    if (!isServerHeaders(this)) throw new ERR_INVALID_THIS('ServerHeaders');
    name = toByteString(name, 'name');
    validateName(name);
    return listGet(this.#list, name);
  }

  getSetCookie() {
    if (!isServerHeaders(this)) throw new ERR_INVALID_THIS('ServerHeaders');
    return listGetAll(this.#list, 'set-cookie');
  }

  has(name) {
    if (!isServerHeaders(this)) throw new ERR_INVALID_THIS('ServerHeaders');
    name = toByteString(name, 'name');
    validateName(name);
    return listContains(this.#list, name);
  }

  set(name, value) {
    if (!isServerHeaders(this)) throw new ERR_INVALID_THIS('ServerHeaders');
    name = toByteString(name, 'name');
    value = normalizeValue(toByteString(value, 'value'));
    this.#validate(name, value);
    listSet(this.#list, name, value);
  }

  entries() {
    if (!isServerHeaders(this)) throw new ERR_INVALID_THIS('ServerHeaders');
    return createHeadersIterator(this, 'key+value');
  }

  keys() {
    if (!isServerHeaders(this)) throw new ERR_INVALID_THIS('ServerHeaders');
    return createHeadersIterator(this, 'key');
  }

  values() {
    if (!isServerHeaders(this)) throw new ERR_INVALID_THIS('ServerHeaders');
    return createHeadersIterator(this, 'value');
  }

  forEach(callback, thisArg = undefined) {
    if (!isServerHeaders(this)) throw new ERR_INVALID_THIS('ServerHeaders');
    if (typeof callback !== 'function') {
      throw new ERR_INVALID_ARG_TYPE('callback', 'Function', callback);
    }
    let pairs = sortAndCombine(this.#list);
    for (let i = 0; i < pairs.length; i++) {
      FunctionPrototypeCall(callback, thisArg, pairs[i][1], pairs[i][0], this);
      // The list may have been modified by the callback.
      pairs = sortAndCombine(this.#list);
    }
  }

  [kInspect](depth, options) {
    if (depth < 0) return 'ServerHeaders {}';
    const obj = { __proto__: null };
    const pairs = sortAndCombine(this.#list);
    for (let i = 0; i < pairs.length; i++) {
      const name = pairs[i][0];
      const prev = obj[name];
      if (prev === undefined) {
        obj[name] = pairs[i][1];
      } else if (ArrayIsArray(prev)) {
        ArrayPrototypePush(prev, pairs[i][1]);
      } else {
        obj[name] = [prev, pairs[i][1]];
      }
    }
    const opts = {
      __proto__: null,
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };
    return `ServerHeaders ${inspect(obj, opts)}`;
  }
}

ObjectDefineProperties(ServerHeaders.prototype, {
  append: kEnumerableProperty,
  delete: kEnumerableProperty,
  get: kEnumerableProperty,
  getSetCookie: kEnumerableProperty,
  has: kEnumerableProperty,
  set: kEnumerableProperty,
  entries: kEnumerableProperty,
  keys: kEnumerableProperty,
  values: kEnumerableProperty,
  forEach: kEnumerableProperty,
  [SymbolIterator]: {
    __proto__: null,
    configurable: true,
    writable: true,
    value: ServerHeaders.prototype.entries,
  },
  [SymbolToStringTag]: {
    __proto__: null,
    configurable: true,
    value: 'ServerHeaders',
  },
});

// Pair iterator, as for a Web IDL `iterable<ByteString, ByteString>`.
const kIteratorPrototype =
  ObjectGetPrototypeOf(ObjectGetPrototypeOf([][SymbolIterator]()));

const kHeadersIteratorPrototype = {
  __proto__: kIteratorPrototype,
  next() {
    const state = this?.[kIteratorState];
    if (state === undefined) {
      throw new ERR_INVALID_THIS('ServerHeaders Iterator');
    }
    const pairs = sortAndCombine(getHeaderList(state.target));
    if (state.index >= pairs.length) {
      return { value: undefined, done: true };
    }
    const pair = pairs[state.index++];
    let value;
    switch (state.kind) {
      case 'key': value = pair[0]; break;
      case 'value': value = pair[1]; break;
      default: value = [pair[0], pair[1]];
    }
    return { value, done: false };
  },
};
ObjectDefineProperties(kHeadersIteratorPrototype, {
  next: kEnumerableProperty,
  [SymbolToStringTag]: {
    __proto__: null,
    configurable: true,
    value: 'ServerHeaders Iterator',
  },
});

const kIteratorState = Symbol('kIteratorState');

function createHeadersIterator(target, kind) {
  const iterator = { [kIteratorState]: { target, kind, index: 0 } };
  ObjectSetPrototypeOf(iterator, kHeadersIteratorPrototype);
  return iterator;
}

/**
 * Creates a server headers object (Section 10.1).
 * @param {Array<[string, string]>} [list]
 * @param {'mutable'|'immutable'} [mutability]
 * @returns {ServerHeaders}
 */
function createServerHeaders(list = [], mutability = 'mutable') {
  return new ServerHeaders(kConstruct, list, mutability === 'immutable');
}

module.exports = {
  ServerHeaders,
  convertHeadersInit,
  copyList,
  createServerHeaders,
  filterTrailers,
  getHeaderList,
  isHeaderName,
  isHeaderValue,
  isProhibitedTrailerName,
  isServerHeaders,
  listContains,
  listDelete,
  listGet,
  listGetAll,
  listSet,
  normalizeValue,
  setHeadersImmutable,
  sortAndCombine,
  toByteString,
};
