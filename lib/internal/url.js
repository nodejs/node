'use strict';

const {
  Array,
  ArrayIsArray,
  ArrayPrototypeJoin,
  ArrayPrototypeMap,
  ArrayPrototypePush,
  ArrayPrototypeReduce,
  ArrayPrototypeSlice,
  Int8Array,
  IteratorPrototype,
  Number,
  ObjectDefineProperties,
  ObjectDefineProperty,
  ObjectGetOwnPropertySymbols,
  ObjectGetPrototypeOf,
  ObjectKeys,
  ObjectPrototypeHasOwnProperty,
  ReflectGetOwnPropertyDescriptor,
  ReflectOwnKeys,
  RegExpPrototypeSymbolReplace,
  StringPrototypeCharAt,
  StringPrototypeCharCodeAt,
  StringPrototypeCodePointAt,
  StringPrototypeIncludes,
  StringPrototypeIndexOf,
  StringPrototypeSlice,
  StringPrototypeSplit,
  StringPrototypeStartsWith,
  Symbol,
  SymbolIterator,
  SymbolToStringTag,
  decodeURIComponent,
} = primordials;

const { inspect } = require('internal/util/inspect');
const {
  encodeStr,
  hexTable,
  isHexTable,
} = require('internal/querystring');

const {
  getConstructorOf,
  removeColors,
  toUSVString,
  kEnumerableProperty,
  SideEffectFreeRegExpPrototypeSymbolReplace,
} = require('internal/util');

const {
  codes: {
    ERR_ARG_NOT_ITERABLE,
    ERR_INVALID_ARG_TYPE,
    ERR_INVALID_ARG_VALUE,
    ERR_INVALID_FILE_URL_HOST,
    ERR_INVALID_FILE_URL_PATH,
    ERR_INVALID_THIS,
    ERR_INVALID_TUPLE,
    ERR_INVALID_URL,
    ERR_INVALID_URL_SCHEME,
    ERR_MISSING_ARGS,
    ERR_NO_CRYPTO,
  },
} = require('internal/errors');
const {
  CHAR_AMPERSAND,
  CHAR_BACKWARD_SLASH,
  CHAR_EQUAL,
  CHAR_FORWARD_SLASH,
  CHAR_LOWERCASE_A,
  CHAR_LOWERCASE_Z,
  CHAR_PERCENT,
  CHAR_PLUS,
} = require('internal/constants');
const path = require('path');

const {
  validateFunction,
} = require('internal/validators');

const querystring = require('querystring');

const { platform } = process;
const isWindows = platform === 'win32';

const {
  domainToASCII: _domainToASCII,
  domainToUnicode: _domainToUnicode,
  parse,
  canParse: _canParse,
  updateUrl,
} = internalBinding('url');

const {
  storeDataObject,
  revokeDataObject,
} = internalBinding('blob');

const FORWARD_SLASH = /\//g;

const context = Symbol('context');
const searchParams = Symbol('query');

const updateActions = {
  kProtocol: 0,
  kHost: 1,
  kHostname: 2,
  kPort: 3,
  kUsername: 4,
  kPassword: 5,
  kPathname: 6,
  kSearch: 7,
  kHash: 8,
  kHref: 9,
};
let blob;
let cryptoRandom;

function lazyBlob() {
  blob ??= require('internal/blob');
  return blob;
}

function lazyCryptoRandom() {
  try {
    cryptoRandom ??= require('internal/crypto/random');
  } catch {
    // If Node.js built without crypto support, we'll fall
    // through here and handle it later.
  }
  return cryptoRandom;
}

// This class provides the internal state of a URL object. An instance of this
// class is stored in every URL object and is accessed internally by setters
// and getters. It roughly corresponds to the concept of a URL record in the
// URL Standard, with a few differences. It is also the object transported to
// the C++ binding.
// Refs: https://url.spec.whatwg.org/#concept-url
class URLContext {
  href = '';
  origin = '';
  protocol = '';
  hostname = '';
  pathname = '';
  search = '';
  username = '';
  password = '';
  port = '';
  hash = '';
}

function isURLSearchParams(self) {
  return self && self[searchParams] && !self[searchParams][searchParams];
}

class URLSearchParams {
  [searchParams] = [];

  // "associated url object"
  [context] = null;

  // URL Standard says the default value is '', but as undefined and '' have
  // the same result, undefined is used to prevent unnecessary parsing.
  // Default parameter is necessary to keep URLSearchParams.length === 0 in
  // accordance with Web IDL spec.
  constructor(init = undefined) {
    if (init == null) {
      // Do nothing
    } else if (typeof init === 'object' || typeof init === 'function') {
      const method = init[SymbolIterator];
      if (method === this[SymbolIterator]) {
        // While the spec does not have this branch, we can use it as a
        // shortcut to avoid having to go through the costly generic iterator.
        const childParams = init[searchParams];
        this[searchParams] = childParams.slice();
      } else if (method != null) {
        // Sequence<sequence<USVString>>
        if (typeof method !== 'function') {
          throw new ERR_ARG_NOT_ITERABLE('Query pairs');
        }

        // The following implementationd differs from the URL specification:
        // Sequences must first be converted from ECMAScript objects before
        // and operations are done on them, and the operation of converting
        // the sequences would first exhaust the iterators. If the iterator
        // returns something invalid in the middle, whether it would be called
        // after that would be an observable change to the users.
        // Exhausting the iterator and later converting them to USVString comes
        // with a significant cost (~40-80%). In order optimize URLSearchParams
        // creation duration, Node.js merges the iteration and converting
        // iterations into a single iteration.
        for (const pair of init) {
          if (pair == null) {
            throw new ERR_INVALID_TUPLE('Each query pair', '[name, value]');
          } else if (ArrayIsArray(pair)) {
            // If innerSequence's size is not 2, then throw a TypeError.
            if (pair.length !== 2) {
              throw new ERR_INVALID_TUPLE('Each query pair', '[name, value]');
            }
            // Append (innerSequence[0], innerSequence[1]) to querys list.
            ArrayPrototypePush(this[searchParams], toUSVString(pair[0]), toUSVString(pair[1]));
          } else {
            if (((typeof pair !== 'object' && typeof pair !== 'function') ||
                typeof pair[SymbolIterator] !== 'function')) {
              throw new ERR_INVALID_TUPLE('Each query pair', '[name, value]');
            }

            let length = 0;

            for (const element of pair) {
              length++;
              ArrayPrototypePush(this[searchParams], toUSVString(element));
            }

            // If innerSequence's size is not 2, then throw a TypeError.
            if (length !== 2) {
              throw new ERR_INVALID_TUPLE('Each query pair', '[name, value]');
            }
          }
        }
      } else {
        // Record<USVString, USVString>
        // Need to use reflection APIs for full spec compliance.
        const visited = {};
        const keys = ReflectOwnKeys(init);
        for (let i = 0; i < keys.length; i++) {
          const key = keys[i];
          const desc = ReflectGetOwnPropertyDescriptor(init, key);
          if (desc !== undefined && desc.enumerable) {
            const typedKey = toUSVString(key);
            const typedValue = toUSVString(init[key]);

            // Two different key may result same after `toUSVString()`, we only
            // leave the later one. Refers to WPT.
            if (visited[typedKey] !== undefined) {
              this[searchParams][visited[typedKey]] = typedValue;
            } else {
              visited[typedKey] = ArrayPrototypePush(this[searchParams],
                                                     typedKey,
                                                     typedValue) - 1;
            }
          }
        }
      }
    } else {
      // https://url.spec.whatwg.org/#dom-urlsearchparams-urlsearchparams
      init = toUSVString(init);
      this[searchParams] = init ? parseParams(init) : [];
    }
  }

  [inspect.custom](recurseTimes, ctx) {
    if (!isURLSearchParams(this))
      throw new ERR_INVALID_THIS('URLSearchParams');

    if (typeof recurseTimes === 'number' && recurseTimes < 0)
      return ctx.stylize('[Object]', 'special');

    const separator = ', ';
    const innerOpts = { ...ctx };
    if (recurseTimes !== null) {
      innerOpts.depth = recurseTimes - 1;
    }
    const innerInspect = (v) => inspect(v, innerOpts);

    const list = this[searchParams];
    const output = [];
    for (let i = 0; i < list.length; i += 2)
      ArrayPrototypePush(
        output,
        `${innerInspect(list[i])} => ${innerInspect(list[i + 1])}`);

    const length = ArrayPrototypeReduce(
      output,
      (prev, cur) => prev + removeColors(cur).length + separator.length,
      -separator.length,
    );
    if (length > ctx.breakLength) {
      return `${this.constructor.name} {\n` +
      `  ${ArrayPrototypeJoin(output, ',\n  ')} }`;
    } else if (output.length) {
      return `${this.constructor.name} { ` +
      `${ArrayPrototypeJoin(output, separator)} }`;
    }
    return `${this.constructor.name} {}`;
  }

  get size() {
    if (!isURLSearchParams(this))
      throw new ERR_INVALID_THIS('URLSearchParams');
    return this[searchParams].length / 2;
  }

  append(name, value) {
    if (!isURLSearchParams(this))
      throw new ERR_INVALID_THIS('URLSearchParams');

    if (arguments.length < 2) {
      throw new ERR_MISSING_ARGS('name', 'value');
    }

    name = toUSVString(name);
    value = toUSVString(value);
    ArrayPrototypePush(this[searchParams], name, value);
    if (this[context]) {
      this[context].search = this.toString();
    }
  }

  delete(name) {
    if (!isURLSearchParams(this))
      throw new ERR_INVALID_THIS('URLSearchParams');

    if (arguments.length < 1) {
      throw new ERR_MISSING_ARGS('name');
    }

    const list = this[searchParams];
    name = toUSVString(name);
    for (let i = 0; i < list.length;) {
      const cur = list[i];
      if (cur === name) {
        list.splice(i, 2);
      } else {
        i += 2;
      }
    }
    if (this[context]) {
      this[context].search = this.toString();
    }
  }

  get(name) {
    if (!isURLSearchParams(this))
      throw new ERR_INVALID_THIS('URLSearchParams');

    if (arguments.length < 1) {
      throw new ERR_MISSING_ARGS('name');
    }

    const list = this[searchParams];
    name = toUSVString(name);
    for (let i = 0; i < list.length; i += 2) {
      if (list[i] === name) {
        return list[i + 1];
      }
    }
    return null;
  }

  getAll(name) {
    if (!isURLSearchParams(this))
      throw new ERR_INVALID_THIS('URLSearchParams');

    if (arguments.length < 1) {
      throw new ERR_MISSING_ARGS('name');
    }

    const list = this[searchParams];
    const values = [];
    name = toUSVString(name);
    for (let i = 0; i < list.length; i += 2) {
      if (list[i] === name) {
        values.push(list[i + 1]);
      }
    }
    return values;
  }

  has(name) {
    if (!isURLSearchParams(this))
      throw new ERR_INVALID_THIS('URLSearchParams');

    if (arguments.length < 1) {
      throw new ERR_MISSING_ARGS('name');
    }

    const list = this[searchParams];
    name = toUSVString(name);
    for (let i = 0; i < list.length; i += 2) {
      if (list[i] === name) {
        return true;
      }
    }
    return false;
  }

  set(name, value) {
    if (!isURLSearchParams(this))
      throw new ERR_INVALID_THIS('URLSearchParams');

    if (arguments.length < 2) {
      throw new ERR_MISSING_ARGS('name', 'value');
    }

    const list = this[searchParams];
    name = toUSVString(name);
    value = toUSVString(value);

    // If there are any name-value pairs whose name is `name`, in `list`, set
    // the value of the first such name-value pair to `value` and remove the
    // others.
    let found = false;
    for (let i = 0; i < list.length;) {
      const cur = list[i];
      if (cur === name) {
        if (!found) {
          list[i + 1] = value;
          found = true;
          i += 2;
        } else {
          list.splice(i, 2);
        }
      } else {
        i += 2;
      }
    }

    // Otherwise, append a new name-value pair whose name is `name` and value
    // is `value`, to `list`.
    if (!found) {
      ArrayPrototypePush(list, name, value);
    }

    if (this[context]) {
      this[context].search = this.toString();
    }
  }

  sort() {
    const a = this[searchParams];
    const len = a.length;

    if (len <= 2) {
      // Nothing needs to be done.
    } else if (len < 100) {
      // 100 is found through testing.
      // Simple stable in-place insertion sort
      // Derived from v8/src/js/array.js
      for (let i = 2; i < len; i += 2) {
        const curKey = a[i];
        const curVal = a[i + 1];
        let j;
        for (j = i - 2; j >= 0; j -= 2) {
          if (a[j] > curKey) {
            a[j + 2] = a[j];
            a[j + 3] = a[j + 1];
          } else {
            break;
          }
        }
        a[j + 2] = curKey;
        a[j + 3] = curVal;
      }
    } else {
      // Bottom-up iterative stable merge sort
      const lBuffer = new Array(len);
      const rBuffer = new Array(len);
      for (let step = 2; step < len; step *= 2) {
        for (let start = 0; start < len - 2; start += 2 * step) {
          const mid = start + step;
          let end = mid + step;
          end = end < len ? end : len;
          if (mid > end)
            continue;
          merge(a, start, mid, end, lBuffer, rBuffer);
        }
      }
    }

    if (this[context]) {
      this[context].search = this.toString();
    }
  }

  // https://heycam.github.io/webidl/#es-iterators
  // Define entries here rather than [Symbol.iterator] as the function name
  // must be set to `entries`.
  entries() {
    if (!isURLSearchParams(this))
      throw new ERR_INVALID_THIS('URLSearchParams');

    return createSearchParamsIterator(this, 'key+value');
  }

  forEach(callback, thisArg = undefined) {
    if (!isURLSearchParams(this))
      throw new ERR_INVALID_THIS('URLSearchParams');

    validateFunction(callback, 'callback');

    let list = this[searchParams];

    let i = 0;
    while (i < list.length) {
      const key = list[i];
      const value = list[i + 1];
      callback.call(thisArg, value, key, this);
      // In case the URL object's `search` is updated
      list = this[searchParams];
      i += 2;
    }
  }

  // https://heycam.github.io/webidl/#es-iterable
  keys() {
    if (!isURLSearchParams(this))
      throw new ERR_INVALID_THIS('URLSearchParams');

    return createSearchParamsIterator(this, 'key');
  }

  values() {
    if (!isURLSearchParams(this))
      throw new ERR_INVALID_THIS('URLSearchParams');

    return createSearchParamsIterator(this, 'value');
  }

  // https://heycam.github.io/webidl/#es-stringifier
  // https://url.spec.whatwg.org/#urlsearchparams-stringification-behavior
  toString() {
    if (!isURLSearchParams(this))
      throw new ERR_INVALID_THIS('URLSearchParams');

    return serializeParams(this[searchParams]);
  }
}

ObjectDefineProperties(URLSearchParams.prototype, {
  append: kEnumerableProperty,
  delete: kEnumerableProperty,
  get: kEnumerableProperty,
  getAll: kEnumerableProperty,
  has: kEnumerableProperty,
  set: kEnumerableProperty,
  size: kEnumerableProperty,
  sort: kEnumerableProperty,
  entries: kEnumerableProperty,
  forEach: kEnumerableProperty,
  keys: kEnumerableProperty,
  values: kEnumerableProperty,
  toString: kEnumerableProperty,
  [SymbolToStringTag]: { __proto__: null, configurable: true, value: 'URLSearchParams' },

  // https://heycam.github.io/webidl/#es-iterable-entries
  [SymbolIterator]: {
    __proto__: null,
    configurable: true,
    writable: true,
    value: URLSearchParams.prototype.entries,
  },
});

function isURL(self) {
  return self != null && ObjectPrototypeHasOwnProperty(self, context);
}

class URL {
  constructor(input, base = undefined) {
    // toUSVString is not needed.
    input = `${input}`;
    this[context] = new URLContext();

    if (base !== undefined) {
      base = `${base}`;
    }

    const isValid = parse(input,
                          base,
                          this.#onParseComplete);

    if (!isValid) {
      throw new ERR_INVALID_URL(input);
    }
  }

  [inspect.custom](depth, opts) {
    if (this == null ||
        ObjectGetPrototypeOf(this[context]) !== URLContext.prototype) {
      throw new ERR_INVALID_THIS('URL');
    }

    if (typeof depth === 'number' && depth < 0)
      return this;

    const constructor = getConstructorOf(this) || URL;
    const obj = { __proto__: { constructor } };

    obj.href = this.href;
    obj.origin = this.origin;
    obj.protocol = this.protocol;
    obj.username = this.username;
    obj.password = this.password;
    obj.host = this.host;
    obj.hostname = this.hostname;
    obj.port = this.port;
    obj.pathname = this.pathname;
    obj.search = this.search;
    obj.searchParams = this.searchParams;
    obj.hash = this.hash;

    if (opts.showHidden) {
      obj[context] = this[context];
    }

    return `${constructor.name} ${inspect(obj, opts)}`;
  }

  #onParseComplete = (href, origin, protocol, hostname, pathname,
                      search, username, password, port, hash) => {
    const ctx = this[context];
    ctx.href = href;
    ctx.origin = origin;
    ctx.protocol = protocol;
    ctx.hostname = hostname;
    ctx.pathname = pathname;
    ctx.search = search;
    ctx.username = username;
    ctx.password = password;
    ctx.port = port;
    ctx.hash = hash;
    if (this[searchParams]) {
      this[searchParams][searchParams] = parseParams(search);
    }
  };

  toString() {
    if (!isURL(this))
      throw new ERR_INVALID_THIS('URL');
    return this[context].href;
  }

  get href() {
    if (!isURL(this))
      throw new ERR_INVALID_THIS('URL');
    return this[context].href;
  }

  set href(value) {
    if (!isURL(this))
      throw new ERR_INVALID_THIS('URL');
    const valid = updateUrl(this[context].href, updateActions.kHref, `${value}`, this.#onParseComplete);
    if (!valid) { throw ERR_INVALID_URL(`${value}`); }
  }

  // readonly
  get origin() {
    if (!isURL(this))
      throw new ERR_INVALID_THIS('URL');
    return this[context].origin;
  }

  get protocol() {
    if (!isURL(this))
      throw new ERR_INVALID_THIS('URL');
    return this[context].protocol;
  }

  set protocol(value) {
    if (!isURL(this))
      throw new ERR_INVALID_THIS('URL');
    updateUrl(this[context].href, updateActions.kProtocol, `${value}`, this.#onParseComplete);
  }

  get username() {
    if (!isURL(this))
      throw new ERR_INVALID_THIS('URL');
    return this[context].username;
  }

  set username(value) {
    if (!isURL(this))
      throw new ERR_INVALID_THIS('URL');
    updateUrl(this[context].href, updateActions.kUsername, `${value}`, this.#onParseComplete);
  }

  get password() {
    if (!isURL(this))
      throw new ERR_INVALID_THIS('URL');
    return this[context].password;
  }

  set password(value) {
    if (!isURL(this))
      throw new ERR_INVALID_THIS('URL');
    updateUrl(this[context].href, updateActions.kPassword, `${value}`, this.#onParseComplete);
  }

  get host() {
    if (!isURL(this))
      throw new ERR_INVALID_THIS('URL');
    const port = this[context].port;
    const suffix = port.length > 0 ? `:${port}` : '';
    return this[context].hostname + suffix;
  }

  set host(value) {
    if (!isURL(this))
      throw new ERR_INVALID_THIS('URL');
    updateUrl(this[context].href, updateActions.kHost, `${value}`, this.#onParseComplete);
  }

  get hostname() {
    if (!isURL(this))
      throw new ERR_INVALID_THIS('URL');
    return this[context].hostname;
  }

  set hostname(value) {
    if (!isURL(this))
      throw new ERR_INVALID_THIS('URL');
    updateUrl(this[context].href, updateActions.kHostname, `${value}`, this.#onParseComplete);
  }

  get port() {
    if (!isURL(this))
      throw new ERR_INVALID_THIS('URL');
    return this[context].port;
  }

  set port(value) {
    if (!isURL(this))
      throw new ERR_INVALID_THIS('URL');
    updateUrl(this[context].href, updateActions.kPort, `${value}`, this.#onParseComplete);
  }

  get pathname() {
    if (!isURL(this))
      throw new ERR_INVALID_THIS('URL');
    return this[context].pathname;
  }

  set pathname(value) {
    if (!isURL(this))
      throw new ERR_INVALID_THIS('URL');
    updateUrl(this[context].href, updateActions.kPathname, `${value}`, this.#onParseComplete);
  }

  get search() {
    if (!isURL(this))
      throw new ERR_INVALID_THIS('URL');
    return this[context].search;
  }

  set search(value) {
    if (!isURL(this))
      throw new ERR_INVALID_THIS('URL');
    updateUrl(this[context].href, updateActions.kSearch, toUSVString(value), this.#onParseComplete);
  }

  // readonly
  get searchParams() {
    if (!isURL(this))
      throw new ERR_INVALID_THIS('URL');
    // Create URLSearchParams on demand to greatly improve the URL performance.
    if (this[searchParams] == null) {
      this[searchParams] = new URLSearchParams(this[context].search);
      this[searchParams][context] = this;
    }
    return this[searchParams];
  }

  get hash() {
    if (!isURL(this))
      throw new ERR_INVALID_THIS('URL');
    return this[context].hash;
  }

  set hash(value) {
    if (!isURL(this))
      throw new ERR_INVALID_THIS('URL');
    updateUrl(this[context].href, updateActions.kHash, `${value}`, this.#onParseComplete);
  }

  toJSON() {
    if (!isURL(this))
      throw new ERR_INVALID_THIS('URL');
    return this[context].href;
  }

  static createObjectURL(obj) {
    const cryptoRandom = lazyCryptoRandom();
    if (cryptoRandom === undefined)
      throw new ERR_NO_CRYPTO();

    const blob = lazyBlob();
    if (!blob.isBlob(obj))
      throw new ERR_INVALID_ARG_TYPE('obj', 'Blob', obj);

    const id = cryptoRandom.randomUUID();

    storeDataObject(id, obj[blob.kHandle], obj.size, obj.type);

    return `blob:nodedata:${id}`;
  }

  static revokeObjectURL(url) {
    url = `${url}`;
    try {
      // TODO(@anonrig): Remove this try/catch by calling `parse` directly.
      const parsed = new URL(url);
      const split = StringPrototypeSplit(parsed.pathname, ':');
      if (split.length === 2)
        revokeDataObject(split[1]);
    } catch {
      // If there's an error, it's ignored.
    }
  }

  static canParse(url, base = undefined) {
    url = `${url}`;

    if (base !== undefined) {
      base = `${base}`;
    }

    return _canParse(url, base);
  }
}

ObjectDefineProperties(URL.prototype, {
  [SymbolToStringTag]: { __proto__: null, configurable: true, value: 'URL' },
  toString: kEnumerableProperty,
  href: kEnumerableProperty,
  origin: kEnumerableProperty,
  protocol: kEnumerableProperty,
  username: kEnumerableProperty,
  password: kEnumerableProperty,
  host: kEnumerableProperty,
  hostname: kEnumerableProperty,
  port: kEnumerableProperty,
  pathname: kEnumerableProperty,
  search: kEnumerableProperty,
  searchParams: kEnumerableProperty,
  hash: kEnumerableProperty,
  toJSON: kEnumerableProperty,
});

ObjectDefineProperties(URL, {
  createObjectURL: kEnumerableProperty,
  revokeObjectURL: kEnumerableProperty,
  canParse: {
    __proto__: null,
    configurable: true,
    writable: true,
    enumerable: true,
  },
});

// application/x-www-form-urlencoded parser
// Ref: https://url.spec.whatwg.org/#concept-urlencoded-parser
function parseParams(qs) {
  const out = [];
  let seenSep = false;
  let buf = '';
  let encoded = false;
  let encodeCheck = 0;
  let i = qs[0] === '?' ? 1 : 0;
  let pairStart = i;
  let lastPos = i;
  for (; i < qs.length; ++i) {
    const code = StringPrototypeCharCodeAt(qs, i);

    // Try matching key/value pair separator
    if (code === CHAR_AMPERSAND) {
      if (pairStart === i) {
        // We saw an empty substring between pair separators
        lastPos = pairStart = i + 1;
        continue;
      }

      if (lastPos < i)
        buf += qs.slice(lastPos, i);
      if (encoded)
        buf = querystring.unescape(buf);
      out.push(buf);

      // If `buf` is the key, add an empty value.
      if (!seenSep)
        out.push('');

      seenSep = false;
      buf = '';
      encoded = false;
      encodeCheck = 0;
      lastPos = pairStart = i + 1;
      continue;
    }

    // Try matching key/value separator (e.g. '=') if we haven't already
    if (!seenSep && code === CHAR_EQUAL) {
      // Key/value separator match!
      if (lastPos < i)
        buf += qs.slice(lastPos, i);
      if (encoded)
        buf = querystring.unescape(buf);
      out.push(buf);

      seenSep = true;
      buf = '';
      encoded = false;
      encodeCheck = 0;
      lastPos = i + 1;
      continue;
    }

    // Handle + and percent decoding.
    if (code === CHAR_PLUS) {
      if (lastPos < i)
        buf += StringPrototypeSlice(qs, lastPos, i);
      buf += ' ';
      lastPos = i + 1;
    } else if (!encoded) {
      // Try to match an (valid) encoded byte (once) to minimize unnecessary
      // calls to string decoding functions
      if (code === CHAR_PERCENT) {
        encodeCheck = 1;
      } else if (encodeCheck > 0) {
        if (isHexTable[code] === 1) {
          if (++encodeCheck === 3) {
            encoded = true;
          }
        } else {
          encodeCheck = 0;
        }
      }
    }
  }

  // Deal with any leftover key or value data

  // There is a trailing &. No more processing is needed.
  if (pairStart === i)
    return out;

  if (lastPos < i)
    buf += StringPrototypeSlice(qs, lastPos, i);
  if (encoded)
    buf = querystring.unescape(buf);
  ArrayPrototypePush(out, buf);

  // If `buf` is the key, add an empty value.
  if (!seenSep)
    ArrayPrototypePush(out, '');

  return out;
}

// Adapted from querystring's implementation.
// Ref: https://url.spec.whatwg.org/#concept-urlencoded-byte-serializer
const noEscape = new Int8Array([
/*
  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, A, B, C, D, E, F
*/
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x00 - 0x0F
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x10 - 0x1F
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, // 0x20 - 0x2F
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, // 0x30 - 0x3F
  0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x40 - 0x4F
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, // 0x50 - 0x5F
  0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x60 - 0x6F
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0,  // 0x70 - 0x7F
]);

// Special version of hexTable that uses `+` for U+0020 SPACE.
const paramHexTable = hexTable.slice();
paramHexTable[0x20] = '+';

// application/x-www-form-urlencoded serializer
// Ref: https://url.spec.whatwg.org/#concept-urlencoded-serializer
function serializeParams(array) {
  const len = array.length;
  if (len === 0)
    return '';

  const firstEncodedParam = encodeStr(array[0], noEscape, paramHexTable);
  const firstEncodedValue = encodeStr(array[1], noEscape, paramHexTable);
  let output = `${firstEncodedParam}=${firstEncodedValue}`;

  for (let i = 2; i < len; i += 2) {
    const encodedParam = encodeStr(array[i], noEscape, paramHexTable);
    const encodedValue = encodeStr(array[i + 1], noEscape, paramHexTable);
    output += `&${encodedParam}=${encodedValue}`;
  }

  return output;
}

// Mainly to mitigate func-name-matching ESLint rule
function defineIDLClass(proto, classStr, obj) {
  // https://heycam.github.io/webidl/#dfn-class-string
  ObjectDefineProperty(proto, SymbolToStringTag, {
    __proto__: null,
    writable: false,
    enumerable: false,
    configurable: true,
    value: classStr,
  });

  // https://heycam.github.io/webidl/#es-operations
  for (const key of ObjectKeys(obj)) {
    ObjectDefineProperty(proto, key, {
      __proto__: null,
      writable: true,
      enumerable: true,
      configurable: true,
      value: obj[key],
    });
  }
  for (const key of ObjectGetOwnPropertySymbols(obj)) {
    ObjectDefineProperty(proto, key, {
      __proto__: null,
      writable: true,
      enumerable: false,
      configurable: true,
      value: obj[key],
    });
  }
}

// for merge sort
function merge(out, start, mid, end, lBuffer, rBuffer) {
  const sizeLeft = mid - start;
  const sizeRight = end - mid;
  let l, r, o;

  for (l = 0; l < sizeLeft; l++)
    lBuffer[l] = out[start + l];
  for (r = 0; r < sizeRight; r++)
    rBuffer[r] = out[mid + r];

  l = 0;
  r = 0;
  o = start;
  while (l < sizeLeft && r < sizeRight) {
    if (lBuffer[l] <= rBuffer[r]) {
      out[o++] = lBuffer[l++];
      out[o++] = lBuffer[l++];
    } else {
      out[o++] = rBuffer[r++];
      out[o++] = rBuffer[r++];
    }
  }
  while (l < sizeLeft)
    out[o++] = lBuffer[l++];
  while (r < sizeRight)
    out[o++] = rBuffer[r++];
}

// https://heycam.github.io/webidl/#dfn-default-iterator-object
function createSearchParamsIterator(target, kind) {
  const iterator = { __proto__: URLSearchParamsIteratorPrototype };
  iterator[context] = {
    target,
    kind,
    index: 0,
  };
  return iterator;
}

// https://heycam.github.io/webidl/#dfn-iterator-prototype-object
const URLSearchParamsIteratorPrototype = { __proto__: IteratorPrototype };

defineIDLClass(URLSearchParamsIteratorPrototype, 'URLSearchParams Iterator', {
  next() {
    if (!this ||
        ObjectGetPrototypeOf(this) !== URLSearchParamsIteratorPrototype) {
      throw new ERR_INVALID_THIS('URLSearchParamsIterator');
    }

    const {
      target,
      kind,
      index,
    } = this[context];
    const values = target[searchParams];
    const len = values.length;
    if (index >= len) {
      return {
        value: undefined,
        done: true,
      };
    }

    const name = values[index];
    const value = values[index + 1];
    this[context].index = index + 2;

    let result;
    if (kind === 'key') {
      result = name;
    } else if (kind === 'value') {
      result = value;
    } else {
      result = [name, value];
    }

    return {
      value: result,
      done: false,
    };
  },
  [inspect.custom](recurseTimes, ctx) {
    if (this == null || this[context] == null || this[context].target == null)
      throw new ERR_INVALID_THIS('URLSearchParamsIterator');

    if (typeof recurseTimes === 'number' && recurseTimes < 0)
      return ctx.stylize('[Object]', 'special');

    const innerOpts = { ...ctx };
    if (recurseTimes !== null) {
      innerOpts.depth = recurseTimes - 1;
    }
    const {
      target,
      kind,
      index,
    } = this[context];
    const output = ArrayPrototypeReduce(
      ArrayPrototypeSlice(target[searchParams], index),
      (prev, cur, i) => {
        const key = i % 2 === 0;
        if (kind === 'key' && key) {
          ArrayPrototypePush(prev, cur);
        } else if (kind === 'value' && !key) {
          ArrayPrototypePush(prev, cur);
        } else if (kind === 'key+value' && !key) {
          ArrayPrototypePush(prev, [target[searchParams][index + i - 1], cur]);
        }
        return prev;
      },
      [],
    );
    const breakLn = StringPrototypeIncludes(inspect(output, innerOpts), '\n');
    const outputStrs = ArrayPrototypeMap(output, (p) => inspect(p, innerOpts));
    let outputStr;
    if (breakLn) {
      outputStr = `\n  ${ArrayPrototypeJoin(outputStrs, ',\n  ')}`;
    } else {
      outputStr = ` ${ArrayPrototypeJoin(outputStrs, ', ')}`;
    }
    return `${this[SymbolToStringTag]} {${outputStr} }`;
  },
});

function domainToASCII(domain) {
  if (arguments.length < 1)
    throw new ERR_MISSING_ARGS('domain');

  // toUSVString is not needed.
  return _domainToASCII(`${domain}`);
}

function domainToUnicode(domain) {
  if (arguments.length < 1)
    throw new ERR_MISSING_ARGS('domain');

  // toUSVString is not needed.
  return _domainToUnicode(`${domain}`);
}

/**
 * Utility function that converts a URL object into an ordinary options object
 * as expected by the `http.request` and `https.request` APIs.
 * @param {URL} url
 * @returns {Record<string, unknown>}
 */
function urlToHttpOptions(url) {
  const { hostname, pathname, port, username, password, search } = url;
  const options = {
    __proto__: null,
    ...url, // In case the url object was extended by the user.
    protocol: url.protocol,
    hostname: hostname && StringPrototypeStartsWith(hostname, '[') ?
      StringPrototypeSlice(hostname, 1, -1) :
      hostname,
    hash: url.hash,
    search: search,
    pathname: pathname,
    path: `${pathname || ''}${search || ''}`,
    href: url.href,
  };
  if (port !== '') {
    options.port = Number(port);
  }
  if (username || password) {
    options.auth = `${decodeURIComponent(username)}:${decodeURIComponent(password)}`;
  }
  return options;
}

function getPathFromURLWin32(url) {
  const hostname = url.hostname;
  let pathname = url.pathname;
  for (let n = 0; n < pathname.length; n++) {
    if (pathname[n] === '%') {
      const third = StringPrototypeCodePointAt(pathname, n + 2) | 0x20;
      if ((pathname[n + 1] === '2' && third === 102) || // 2f 2F /
          (pathname[n + 1] === '5' && third === 99)) {  // 5c 5C \
        throw new ERR_INVALID_FILE_URL_PATH(
          'must not include encoded \\ or / characters',
        );
      }
    }
  }
  pathname = SideEffectFreeRegExpPrototypeSymbolReplace(FORWARD_SLASH, pathname, '\\');
  pathname = decodeURIComponent(pathname);
  if (hostname !== '') {
    // If hostname is set, then we have a UNC path
    // Pass the hostname through domainToUnicode just in case
    // it is an IDN using punycode encoding. We do not need to worry
    // about percent encoding because the URL parser will have
    // already taken care of that for us. Note that this only
    // causes IDNs with an appropriate `xn--` prefix to be decoded.
    return `\\\\${domainToUnicode(hostname)}${pathname}`;
  }
  // Otherwise, it's a local path that requires a drive letter
  const letter = StringPrototypeCodePointAt(pathname, 1) | 0x20;
  const sep = StringPrototypeCharAt(pathname, 2);
  if (letter < CHAR_LOWERCASE_A || letter > CHAR_LOWERCASE_Z ||   // a..z A..Z
      (sep !== ':')) {
    throw new ERR_INVALID_FILE_URL_PATH('must be absolute');
  }
  return StringPrototypeSlice(pathname, 1);
}

function getPathFromURLPosix(url) {
  if (url.hostname !== '') {
    throw new ERR_INVALID_FILE_URL_HOST(platform);
  }
  const pathname = url.pathname;
  for (let n = 0; n < pathname.length; n++) {
    if (pathname[n] === '%') {
      const third = StringPrototypeCodePointAt(pathname, n + 2) | 0x20;
      if (pathname[n + 1] === '2' && third === 102) {
        throw new ERR_INVALID_FILE_URL_PATH(
          'must not include encoded / characters',
        );
      }
    }
  }
  return decodeURIComponent(pathname);
}

function fileURLToPath(path) {
  if (typeof path === 'string')
    path = new URL(path);
  else if (!isURLInstance(path))
    throw new ERR_INVALID_ARG_TYPE('path', ['string', 'URL'], path);
  if (path.protocol !== 'file:')
    throw new ERR_INVALID_URL_SCHEME('file');
  return isWindows ? getPathFromURLWin32(path) : getPathFromURLPosix(path);
}

// The following characters are percent-encoded when converting from file path
// to URL:
// - %: The percent character is the only character not encoded by the
//        `pathname` setter.
// - \: Backslash is encoded on non-windows platforms since it's a valid
//      character but the `pathname` setters replaces it by a forward slash.
// - LF: The newline character is stripped out by the `pathname` setter.
//       (See whatwg/url#419)
// - CR: The carriage return character is also stripped out by the `pathname`
//       setter.
// - TAB: The tab character is also stripped out by the `pathname` setter.
const percentRegEx = /%/g;
const backslashRegEx = /\\/g;
const newlineRegEx = /\n/g;
const carriageReturnRegEx = /\r/g;
const tabRegEx = /\t/g;

function encodePathChars(filepath) {
  if (StringPrototypeIncludes(filepath, '%'))
    filepath = RegExpPrototypeSymbolReplace(percentRegEx, filepath, '%25');
  // In posix, backslash is a valid character in paths:
  if (!isWindows && StringPrototypeIncludes(filepath, '\\'))
    filepath = RegExpPrototypeSymbolReplace(backslashRegEx, filepath, '%5C');
  if (StringPrototypeIncludes(filepath, '\n'))
    filepath = RegExpPrototypeSymbolReplace(newlineRegEx, filepath, '%0A');
  if (StringPrototypeIncludes(filepath, '\r'))
    filepath = RegExpPrototypeSymbolReplace(carriageReturnRegEx, filepath, '%0D');
  if (StringPrototypeIncludes(filepath, '\t'))
    filepath = RegExpPrototypeSymbolReplace(tabRegEx, filepath, '%09');
  return filepath;
}

function pathToFileURL(filepath) {
  const outURL = new URL('file://');
  if (isWindows && StringPrototypeStartsWith(filepath, '\\\\')) {
    // UNC path format: \\server\share\resource
    const hostnameEndIndex = StringPrototypeIndexOf(filepath, '\\', 2);
    if (hostnameEndIndex === -1) {
      throw new ERR_INVALID_ARG_VALUE(
        'filepath',
        filepath,
        'Missing UNC resource path',
      );
    }
    if (hostnameEndIndex === 2) {
      throw new ERR_INVALID_ARG_VALUE(
        'filepath',
        filepath,
        'Empty UNC servername',
      );
    }
    const hostname = StringPrototypeSlice(filepath, 2, hostnameEndIndex);
    outURL.hostname = domainToASCII(hostname);
    outURL.pathname = encodePathChars(
      RegExpPrototypeSymbolReplace(backslashRegEx, StringPrototypeSlice(filepath, hostnameEndIndex), '/'));
  } else {
    let resolved = path.resolve(filepath);
    // path.resolve strips trailing slashes so we must add them back
    const filePathLast = StringPrototypeCharCodeAt(filepath,
                                                   filepath.length - 1);
    if ((filePathLast === CHAR_FORWARD_SLASH ||
         (isWindows && filePathLast === CHAR_BACKWARD_SLASH)) &&
        resolved[resolved.length - 1] !== path.sep)
      resolved += '/';
    outURL.pathname = encodePathChars(resolved);
  }
  return outURL;
}

function isURLInstance(fileURLOrPath) {
  return fileURLOrPath != null && fileURLOrPath.href && fileURLOrPath.origin;
}

function toPathIfFileURL(fileURLOrPath) {
  if (!isURLInstance(fileURLOrPath))
    return fileURLOrPath;
  return fileURLToPath(fileURLOrPath);
}

module.exports = {
  toUSVString,
  fileURLToPath,
  pathToFileURL,
  toPathIfFileURL,
  isURLInstance,
  URL,
  URLSearchParams,
  domainToASCII,
  domainToUnicode,
  urlToHttpOptions,
  encodeStr,
  isURL,
};
