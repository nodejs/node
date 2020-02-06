'use strict';

const {
  Array,
  Number,
  ObjectCreate,
  ObjectDefineProperties,
  ObjectDefineProperty,
  ObjectGetOwnPropertySymbols,
  ObjectGetPrototypeOf,
  ObjectKeys,
  ReflectGetOwnPropertyDescriptor,
  ReflectOwnKeys,
  Symbol,
  SymbolIterator,
  SymbolToStringTag,
} = primordials;

const { inspect } = require('internal/util/inspect');
const {
  encodeStr,
  hexTable,
  isHexTable
} = require('internal/querystring');

const { getConstructorOf, removeColors } = require('internal/util');
const {
  ERR_ARG_NOT_ITERABLE,
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_CALLBACK,
  ERR_INVALID_FILE_URL_HOST,
  ERR_INVALID_FILE_URL_PATH,
  ERR_INVALID_THIS,
  ERR_INVALID_TUPLE,
  ERR_INVALID_URL,
  ERR_INVALID_URL_SCHEME,
  ERR_MISSING_ARGS
} = require('internal/errors').codes;
const {
  CHAR_AMPERSAND,
  CHAR_BACKWARD_SLASH,
  CHAR_EQUAL,
  CHAR_FORWARD_SLASH,
  CHAR_LOWERCASE_A,
  CHAR_LOWERCASE_Z,
  CHAR_PERCENT,
  CHAR_PLUS
} = require('internal/constants');
const path = require('path');

// Lazy loaded for startup performance.
let querystring;

const { platform } = process;
const isWindows = platform === 'win32';

const {
  domainToASCII: _domainToASCII,
  domainToUnicode: _domainToUnicode,
  encodeAuth,
  toUSVString: _toUSVString,
  parse,
  setURLConstructor,
  URL_FLAGS_CANNOT_BE_BASE,
  URL_FLAGS_HAS_FRAGMENT,
  URL_FLAGS_HAS_HOST,
  URL_FLAGS_HAS_PASSWORD,
  URL_FLAGS_HAS_PATH,
  URL_FLAGS_HAS_QUERY,
  URL_FLAGS_HAS_USERNAME,
  URL_FLAGS_IS_DEFAULT_SCHEME_PORT,
  URL_FLAGS_SPECIAL,
  kFragment,
  kHost,
  kHostname,
  kPathStart,
  kPort,
  kQuery,
  kSchemeStart
} = internalBinding('url');

const context = Symbol('context');
const cannotBeBase = Symbol('cannot-be-base');
const cannotHaveUsernamePasswordPort =
    Symbol('cannot-have-username-password-port');
const special = Symbol('special');
const searchParams = Symbol('query');
const kFormat = Symbol('format');

// https://tc39.github.io/ecma262/#sec-%iteratorprototype%-object
const IteratorPrototype = ObjectGetPrototypeOf(
  ObjectGetPrototypeOf([][SymbolIterator]())
);

const unpairedSurrogateRe =
    /(?:[^\uD800-\uDBFF]|^)[\uDC00-\uDFFF]|[\uD800-\uDBFF](?![\uDC00-\uDFFF])/;
function toUSVString(val) {
  const str = `${val}`;
  // As of V8 5.5, `str.search()` (and `unpairedSurrogateRe[@@search]()`) are
  // slower than `unpairedSurrogateRe.exec()`.
  const match = unpairedSurrogateRe.exec(str);
  if (!match)
    return str;
  return _toUSVString(str, match.index);
}

// Refs: https://html.spec.whatwg.org/multipage/browsers.html#concept-origin-opaque
const kOpaqueOrigin = 'null';

// Refs: https://html.spec.whatwg.org/multipage/browsers.html#ascii-serialisation-of-an-origin
function serializeTupleOrigin(scheme, host, port) {
  return `${scheme}//${host}${port === null ? '' : `:${port}`}`;
}

// This class provides the internal state of a URL object. An instance of this
// class is stored in every URL object and is accessed internally by setters
// and getters. It roughly corresponds to the concept of a URL record in the
// URL Standard, with a few differences. It is also the object transported to
// the C++ binding.
// Refs: https://url.spec.whatwg.org/#concept-url
class URLContext {
  constructor() {
    this.flags = 0;
    this.scheme = ':';
    this.username = '';
    this.password = '';
    this.host = null;
    this.port = null;
    this.path = [];
    this.query = null;
    this.fragment = null;
  }
}

class URLSearchParams {
  // URL Standard says the default value is '', but as undefined and '' have
  // the same result, undefined is used to prevent unnecessary parsing.
  // Default parameter is necessary to keep URLSearchParams.length === 0 in
  // accordance with Web IDL spec.
  constructor(init = undefined) {
    if (init === null || init === undefined) {
      this[searchParams] = [];
    } else if (typeof init === 'object' || typeof init === 'function') {
      const method = init[SymbolIterator];
      if (method === this[SymbolIterator]) {
        // While the spec does not have this branch, we can use it as a
        // shortcut to avoid having to go through the costly generic iterator.
        const childParams = init[searchParams];
        this[searchParams] = childParams.slice();
      } else if (method !== null && method !== undefined) {
        if (typeof method !== 'function') {
          throw new ERR_ARG_NOT_ITERABLE('Query pairs');
        }

        // Sequence<sequence<USVString>>
        // Note: per spec we have to first exhaust the lists then process them
        const pairs = [];
        for (const pair of init) {
          if ((typeof pair !== 'object' && typeof pair !== 'function') ||
              pair === null ||
              typeof pair[SymbolIterator] !== 'function') {
            throw new ERR_INVALID_TUPLE('Each query pair', '[name, value]');
          }
          const convertedPair = [];
          for (const element of pair)
            convertedPair.push(toUSVString(element));
          pairs.push(convertedPair);
        }

        this[searchParams] = [];
        for (const pair of pairs) {
          if (pair.length !== 2) {
            throw new ERR_INVALID_TUPLE('Each query pair', '[name, value]');
          }
          this[searchParams].push(pair[0], pair[1]);
        }
      } else {
        // Record<USVString, USVString>
        // Need to use reflection APIs for full spec compliance.
        this[searchParams] = [];
        const keys = ReflectOwnKeys(init);
        for (let i = 0; i < keys.length; i++) {
          const key = keys[i];
          const desc = ReflectGetOwnPropertyDescriptor(init, key);
          if (desc !== undefined && desc.enumerable) {
            const typedKey = toUSVString(key);
            const typedValue = toUSVString(init[key]);
            this[searchParams].push(typedKey, typedValue);
          }
        }
      }
    } else {
      // USVString
      init = toUSVString(init);
      if (init[0] === '?') init = init.slice(1);
      initSearchParams(this, init);
    }

    // "associated url object"
    this[context] = null;
  }

  [inspect.custom](recurseTimes, ctx) {
    if (!this || !this[searchParams] || this[searchParams][searchParams]) {
      throw new ERR_INVALID_THIS('URLSearchParams');
    }

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
      output.push(`${innerInspect(list[i])} => ${innerInspect(list[i + 1])}`);

    const length = output.reduce(
      (prev, cur) => prev + removeColors(cur).length + separator.length,
      -separator.length
    );
    if (length > ctx.breakLength) {
      return `${this.constructor.name} {\n  ${output.join(',\n  ')} }`;
    } else if (output.length) {
      return `${this.constructor.name} { ${output.join(separator)} }`;
    } else {
      return `${this.constructor.name} {}`;
    }
  }
}

function onParseComplete(flags, protocol, username, password,
                         host, port, path, query, fragment) {
  const ctx = this[context];
  ctx.flags = flags;
  ctx.scheme = protocol;
  ctx.username = (flags & URL_FLAGS_HAS_USERNAME) !== 0 ? username : '';
  ctx.password = (flags & URL_FLAGS_HAS_PASSWORD) !== 0 ? password : '';
  ctx.port = port;
  ctx.path = (flags & URL_FLAGS_HAS_PATH) !== 0 ? path : [];
  ctx.query = query;
  ctx.fragment = fragment;
  ctx.host = host;
  if (!this[searchParams]) { // Invoked from URL constructor
    this[searchParams] = new URLSearchParams();
    this[searchParams][context] = this;
  }
  initSearchParams(this[searchParams], query);
}

function onParseError(flags, input) {
  throw new ERR_INVALID_URL(input);
}

function onParseProtocolComplete(flags, protocol, username, password,
                                 host, port, path, query, fragment) {
  const ctx = this[context];
  if ((flags & URL_FLAGS_SPECIAL) !== 0) {
    ctx.flags |= URL_FLAGS_SPECIAL;
  } else {
    ctx.flags &= ~URL_FLAGS_SPECIAL;
  }
  ctx.scheme = protocol;
  ctx.port = port;
}

function onParseHostnameComplete(flags, protocol, username, password,
                                 host, port, path, query, fragment) {
  const ctx = this[context];
  if ((flags & URL_FLAGS_HAS_HOST) !== 0) {
    ctx.host = host;
    ctx.flags |= URL_FLAGS_HAS_HOST;
  } else {
    ctx.host = null;
    ctx.flags &= ~URL_FLAGS_HAS_HOST;
  }
}

function onParsePortComplete(flags, protocol, username, password,
                             host, port, path, query, fragment) {
  this[context].port = port;
}

function onParseHostComplete(flags, protocol, username, password,
                             host, port, path, query, fragment) {
  onParseHostnameComplete.apply(this, arguments);
  if (port !== null || ((flags & URL_FLAGS_IS_DEFAULT_SCHEME_PORT) !== 0))
    onParsePortComplete.apply(this, arguments);
}

function onParsePathComplete(flags, protocol, username, password,
                             host, port, path, query, fragment) {
  const ctx = this[context];
  if ((flags & URL_FLAGS_HAS_PATH) !== 0) {
    ctx.path = path;
    ctx.flags |= URL_FLAGS_HAS_PATH;
  } else {
    ctx.path = [];
    ctx.flags &= ~URL_FLAGS_HAS_PATH;
  }

  // The C++ binding may set host to empty string.
  if ((flags & URL_FLAGS_HAS_HOST) !== 0) {
    ctx.host = host;
    ctx.flags |= URL_FLAGS_HAS_HOST;
  }
}

function onParseSearchComplete(flags, protocol, username, password,
                               host, port, path, query, fragment) {
  this[context].query = query;
}

function onParseHashComplete(flags, protocol, username, password,
                             host, port, path, query, fragment) {
  this[context].fragment = fragment;
}

class URL {
  constructor(input, base) {
    // toUSVString is not needed.
    input = `${input}`;
    let base_context;
    if (base !== undefined) {
      base_context = new URL(base)[context];
    }
    this[context] = new URLContext();
    parse(input, -1, base_context, undefined, onParseComplete.bind(this),
          onParseError);
  }

  get [special]() {
    return (this[context].flags & URL_FLAGS_SPECIAL) !== 0;
  }

  get [cannotBeBase]() {
    return (this[context].flags & URL_FLAGS_CANNOT_BE_BASE) !== 0;
  }

  // https://url.spec.whatwg.org/#cannot-have-a-username-password-port
  get [cannotHaveUsernamePasswordPort]() {
    const { host, scheme } = this[context];
    return ((host == null || host === '') ||
            this[cannotBeBase] ||
            scheme === 'file:');
  }

  [inspect.custom](depth, opts) {
    if (this == null ||
        ObjectGetPrototypeOf(this[context]) !== URLContext.prototype) {
      throw new ERR_INVALID_THIS('URL');
    }

    if (typeof depth === 'number' && depth < 0)
      return this;

    const ctor = getConstructorOf(this);

    const obj = ObjectCreate({
      constructor: ctor === null ? URL : ctor
    });

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
      obj.cannotBeBase = this[cannotBeBase];
      obj.special = this[special];
      obj[context] = this[context];
    }

    return inspect(obj, opts);
  }
}

ObjectDefineProperties(URL.prototype, {
  [kFormat]: {
    enumerable: false,
    configurable: false,
    // eslint-disable-next-line func-name-matching
    value: function format(options) {
      if (options && typeof options !== 'object')
        throw new ERR_INVALID_ARG_TYPE('options', 'Object', options);
      options = {
        fragment: true,
        unicode: false,
        search: true,
        auth: true,
        ...options
      };
      const ctx = this[context];
      let ret = ctx.scheme;
      if (ctx.host !== null) {
        ret += '//';
        const has_username = ctx.username !== '';
        const has_password = ctx.password !== '';
        if (options.auth && (has_username || has_password)) {
          if (has_username)
            ret += ctx.username;
          if (has_password)
            ret += `:${ctx.password}`;
          ret += '@';
        }
        ret += options.unicode ?
          domainToUnicode(this.hostname) : this.hostname;
        if (ctx.port !== null)
          ret += `:${ctx.port}`;
      } else if (ctx.scheme === 'file:') {
        ret += '//';
      }
      if (this.pathname)
        ret += this.pathname;
      if (options.search && ctx.query !== null)
        ret += `?${ctx.query}`;
      if (options.fragment && ctx.fragment !== null)
        ret += `#${ctx.fragment}`;
      return ret;
    }
  },
  [SymbolToStringTag]: {
    configurable: true,
    value: 'URL'
  },
  toString: {
    // https://heycam.github.io/webidl/#es-stringifier
    writable: true,
    enumerable: true,
    configurable: true,
    // eslint-disable-next-line func-name-matching
    value: function toString() {
      return this[kFormat]({});
    }
  },
  href: {
    enumerable: true,
    configurable: true,
    get() {
      return this[kFormat]({});
    },
    set(input) {
      // toUSVString is not needed.
      input = `${input}`;
      parse(input, -1, undefined, undefined, onParseComplete.bind(this),
            onParseError);
    }
  },
  origin: {  // readonly
    enumerable: true,
    configurable: true,
    get() {
      // Refs: https://url.spec.whatwg.org/#concept-url-origin
      const ctx = this[context];
      switch (ctx.scheme) {
        case 'blob:':
          if (ctx.path.length > 0) {
            try {
              return (new URL(ctx.path[0])).origin;
            } catch {
              // Fall through... do nothing
            }
          }
          return kOpaqueOrigin;
        case 'ftp:':
        case 'gopher:':
        case 'http:':
        case 'https:':
        case 'ws:':
        case 'wss:':
          return serializeTupleOrigin(ctx.scheme, ctx.host, ctx.port);
      }
      return kOpaqueOrigin;
    }
  },
  protocol: {
    enumerable: true,
    configurable: true,
    get() {
      return this[context].scheme;
    },
    set(scheme) {
      // toUSVString is not needed.
      scheme = `${scheme}`;
      if (scheme.length === 0)
        return;
      const ctx = this[context];
      if (ctx.scheme === 'file:' &&
          (ctx.host === '' || ctx.host === null)) {
        return;
      }
      parse(scheme, kSchemeStart, null, ctx,
            onParseProtocolComplete.bind(this));
    }
  },
  username: {
    enumerable: true,
    configurable: true,
    get() {
      return this[context].username;
    },
    set(username) {
      // toUSVString is not needed.
      username = `${username}`;
      if (this[cannotHaveUsernamePasswordPort])
        return;
      const ctx = this[context];
      if (username === '') {
        ctx.username = '';
        ctx.flags &= ~URL_FLAGS_HAS_USERNAME;
        return;
      }
      ctx.username = encodeAuth(username);
      ctx.flags |= URL_FLAGS_HAS_USERNAME;
    }
  },
  password: {
    enumerable: true,
    configurable: true,
    get() {
      return this[context].password;
    },
    set(password) {
      // toUSVString is not needed.
      password = `${password}`;
      if (this[cannotHaveUsernamePasswordPort])
        return;
      const ctx = this[context];
      if (password === '') {
        ctx.password = '';
        ctx.flags &= ~URL_FLAGS_HAS_PASSWORD;
        return;
      }
      ctx.password = encodeAuth(password);
      ctx.flags |= URL_FLAGS_HAS_PASSWORD;
    }
  },
  host: {
    enumerable: true,
    configurable: true,
    get() {
      const ctx = this[context];
      let ret = ctx.host || '';
      if (ctx.port !== null)
        ret += `:${ctx.port}`;
      return ret;
    },
    set(host) {
      const ctx = this[context];
      // toUSVString is not needed.
      host = `${host}`;
      if (this[cannotBeBase]) {
        // Cannot set the host if cannot-be-base is set
        return;
      }
      parse(host, kHost, null, ctx, onParseHostComplete.bind(this));
    }
  },
  hostname: {
    enumerable: true,
    configurable: true,
    get() {
      return this[context].host || '';
    },
    set(host) {
      const ctx = this[context];
      // toUSVString is not needed.
      host = `${host}`;
      if (this[cannotBeBase]) {
        // Cannot set the host if cannot-be-base is set
        return;
      }
      parse(host, kHostname, null, ctx, onParseHostnameComplete.bind(this));
    }
  },
  port: {
    enumerable: true,
    configurable: true,
    get() {
      const port = this[context].port;
      return port === null ? '' : String(port);
    },
    set(port) {
      // toUSVString is not needed.
      port = `${port}`;
      if (this[cannotHaveUsernamePasswordPort])
        return;
      const ctx = this[context];
      if (port === '') {
        ctx.port = null;
        return;
      }
      parse(port, kPort, null, ctx, onParsePortComplete.bind(this));
    }
  },
  pathname: {
    enumerable: true,
    configurable: true,
    get() {
      const ctx = this[context];
      if (this[cannotBeBase])
        return ctx.path[0];
      if (ctx.path.length === 0)
        return '';
      return `/${ctx.path.join('/')}`;
    },
    set(path) {
      // toUSVString is not needed.
      path = `${path}`;
      if (this[cannotBeBase])
        return;
      parse(path, kPathStart, null, this[context],
            onParsePathComplete.bind(this));
    }
  },
  search: {
    enumerable: true,
    configurable: true,
    get() {
      const { query } = this[context];
      if (query === null || query === '')
        return '';
      return `?${query}`;
    },
    set(search) {
      const ctx = this[context];
      search = toUSVString(search);
      if (search === '') {
        ctx.query = null;
        ctx.flags &= ~URL_FLAGS_HAS_QUERY;
      } else {
        if (search[0] === '?') search = search.slice(1);
        ctx.query = '';
        ctx.flags |= URL_FLAGS_HAS_QUERY;
        if (search) {
          parse(search, kQuery, null, ctx, onParseSearchComplete.bind(this));
        }
      }
      initSearchParams(this[searchParams], search);
    }
  },
  searchParams: {  // readonly
    enumerable: true,
    configurable: true,
    get() {
      return this[searchParams];
    }
  },
  hash: {
    enumerable: true,
    configurable: true,
    get() {
      const { fragment } = this[context];
      if (fragment === null || fragment === '')
        return '';
      return `#${fragment}`;
    },
    set(hash) {
      const ctx = this[context];
      // toUSVString is not needed.
      hash = `${hash}`;
      if (!hash) {
        ctx.fragment = null;
        ctx.flags &= ~URL_FLAGS_HAS_FRAGMENT;
        return;
      }
      if (hash[0] === '#') hash = hash.slice(1);
      ctx.fragment = '';
      ctx.flags |= URL_FLAGS_HAS_FRAGMENT;
      parse(hash, kFragment, null, ctx, onParseHashComplete.bind(this));
    }
  },
  toJSON: {
    writable: true,
    enumerable: true,
    configurable: true,
    // eslint-disable-next-line func-name-matching
    value: function toJSON() {
      return this[kFormat]({});
    }
  }
});

function update(url, params) {
  if (!url)
    return;

  const ctx = url[context];
  const serializedParams = params.toString();
  if (serializedParams) {
    ctx.query = serializedParams;
    ctx.flags |= URL_FLAGS_HAS_QUERY;
  } else {
    ctx.query = null;
    ctx.flags &= ~URL_FLAGS_HAS_QUERY;
  }
}

function initSearchParams(url, init) {
  if (!init) {
    url[searchParams] = [];
    return;
  }
  url[searchParams] = parseParams(init);
}

// application/x-www-form-urlencoded parser
// Ref: https://url.spec.whatwg.org/#concept-urlencoded-parser
function parseParams(qs) {
  const out = [];
  let pairStart = 0;
  let lastPos = 0;
  let seenSep = false;
  let buf = '';
  let encoded = false;
  let encodeCheck = 0;
  let i;
  for (i = 0; i < qs.length; ++i) {
    const code = qs.charCodeAt(i);

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
        buf += qs.slice(lastPos, i);
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
            querystring = require('querystring');
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
    buf += qs.slice(lastPos, i);
  if (encoded)
    buf = querystring.unescape(buf);
  out.push(buf);

  // If `buf` is the key, add an empty value.
  if (!seenSep)
    out.push('');

  return out;
}

// Adapted from querystring's implementation.
// Ref: https://url.spec.whatwg.org/#concept-urlencoded-byte-serializer
const noEscape = [
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
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0  // 0x70 - 0x7F
];

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
    writable: false,
    enumerable: false,
    configurable: true,
    value: classStr
  });

  // https://heycam.github.io/webidl/#es-operations
  for (const key of ObjectKeys(obj)) {
    ObjectDefineProperty(proto, key, {
      writable: true,
      enumerable: true,
      configurable: true,
      value: obj[key]
    });
  }
  for (const key of ObjectGetOwnPropertySymbols(obj)) {
    ObjectDefineProperty(proto, key, {
      writable: true,
      enumerable: false,
      configurable: true,
      value: obj[key]
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

defineIDLClass(URLSearchParams.prototype, 'URLSearchParams', {
  append(name, value) {
    if (!this || !this[searchParams] || this[searchParams][searchParams]) {
      throw new ERR_INVALID_THIS('URLSearchParams');
    }
    if (arguments.length < 2) {
      throw new ERR_MISSING_ARGS('name', 'value');
    }

    name = toUSVString(name);
    value = toUSVString(value);
    this[searchParams].push(name, value);
    update(this[context], this);
  },

  delete(name) {
    if (!this || !this[searchParams] || this[searchParams][searchParams]) {
      throw new ERR_INVALID_THIS('URLSearchParams');
    }
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
    update(this[context], this);
  },

  get(name) {
    if (!this || !this[searchParams] || this[searchParams][searchParams]) {
      throw new ERR_INVALID_THIS('URLSearchParams');
    }
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
  },

  getAll(name) {
    if (!this || !this[searchParams] || this[searchParams][searchParams]) {
      throw new ERR_INVALID_THIS('URLSearchParams');
    }
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
  },

  has(name) {
    if (!this || !this[searchParams] || this[searchParams][searchParams]) {
      throw new ERR_INVALID_THIS('URLSearchParams');
    }
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
  },

  set(name, value) {
    if (!this || !this[searchParams] || this[searchParams][searchParams]) {
      throw new ERR_INVALID_THIS('URLSearchParams');
    }
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
      list.push(name, value);
    }

    update(this[context], this);
  },

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

    update(this[context], this);
  },

  // https://heycam.github.io/webidl/#es-iterators
  // Define entries here rather than [Symbol.iterator] as the function name
  // must be set to `entries`.
  entries() {
    if (!this || !this[searchParams] || this[searchParams][searchParams]) {
      throw new ERR_INVALID_THIS('URLSearchParams');
    }

    return createSearchParamsIterator(this, 'key+value');
  },

  forEach(callback, thisArg = undefined) {
    if (!this || !this[searchParams] || this[searchParams][searchParams]) {
      throw new ERR_INVALID_THIS('URLSearchParams');
    }
    if (typeof callback !== 'function') {
      throw new ERR_INVALID_CALLBACK(callback);
    }

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
  },

  // https://heycam.github.io/webidl/#es-iterable
  keys() {
    if (!this || !this[searchParams] || this[searchParams][searchParams]) {
      throw new ERR_INVALID_THIS('URLSearchParams');
    }

    return createSearchParamsIterator(this, 'key');
  },

  values() {
    if (!this || !this[searchParams] || this[searchParams][searchParams]) {
      throw new ERR_INVALID_THIS('URLSearchParams');
    }

    return createSearchParamsIterator(this, 'value');
  },

  // https://heycam.github.io/webidl/#es-stringifier
  // https://url.spec.whatwg.org/#urlsearchparams-stringification-behavior
  toString() {
    if (!this || !this[searchParams] || this[searchParams][searchParams]) {
      throw new ERR_INVALID_THIS('URLSearchParams');
    }

    return serializeParams(this[searchParams]);
  }
});

// https://heycam.github.io/webidl/#es-iterable-entries
ObjectDefineProperty(URLSearchParams.prototype, SymbolIterator, {
  writable: true,
  configurable: true,
  value: URLSearchParams.prototype.entries
});

// https://heycam.github.io/webidl/#dfn-default-iterator-object
function createSearchParamsIterator(target, kind) {
  const iterator = ObjectCreate(URLSearchParamsIteratorPrototype);
  iterator[context] = {
    target,
    kind,
    index: 0
  };
  return iterator;
}

// https://heycam.github.io/webidl/#dfn-iterator-prototype-object
const URLSearchParamsIteratorPrototype = ObjectCreate(IteratorPrototype);

defineIDLClass(URLSearchParamsIteratorPrototype, 'URLSearchParams Iterator', {
  next() {
    if (!this ||
        ObjectGetPrototypeOf(this) !== URLSearchParamsIteratorPrototype) {
      throw new ERR_INVALID_THIS('URLSearchParamsIterator');
    }

    const {
      target,
      kind,
      index
    } = this[context];
    const values = target[searchParams];
    const len = values.length;
    if (index >= len) {
      return {
        value: undefined,
        done: true
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
      done: false
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
      index
    } = this[context];
    const output = target[searchParams].slice(index).reduce((prev, cur, i) => {
      const key = i % 2 === 0;
      if (kind === 'key' && key) {
        prev.push(cur);
      } else if (kind === 'value' && !key) {
        prev.push(cur);
      } else if (kind === 'key+value' && !key) {
        prev.push([target[searchParams][index + i - 1], cur]);
      }
      return prev;
    }, []);
    const breakLn = inspect(output, innerOpts).includes('\n');
    const outputStrs = output.map((p) => inspect(p, innerOpts));
    let outputStr;
    if (breakLn) {
      outputStr = `\n  ${outputStrs.join(',\n  ')}`;
    } else {
      outputStr = ` ${outputStrs.join(', ')}`;
    }
    return `${this[SymbolToStringTag]} {${outputStr} }`;
  }
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

// Utility function that converts a URL object into an ordinary
// options object as expected by the http.request and https.request
// APIs.
function urlToOptions(url) {
  const options = {
    protocol: url.protocol,
    hostname: typeof url.hostname === 'string' && url.hostname.startsWith('[') ?
      url.hostname.slice(1, -1) :
      url.hostname,
    hash: url.hash,
    search: url.search,
    pathname: url.pathname,
    path: `${url.pathname || ''}${url.search || ''}`,
    href: url.href
  };
  if (url.port !== '') {
    options.port = Number(url.port);
  }
  if (url.username || url.password) {
    options.auth = `${url.username}:${url.password}`;
  }
  return options;
}

const forwardSlashRegEx = /\//g;

function getPathFromURLWin32(url) {
  const hostname = url.hostname;
  let pathname = url.pathname;
  for (let n = 0; n < pathname.length; n++) {
    if (pathname[n] === '%') {
      const third = pathname.codePointAt(n + 2) | 0x20;
      if ((pathname[n + 1] === '2' && third === 102) || // 2f 2F /
          (pathname[n + 1] === '5' && third === 99)) {  // 5c 5C \
        throw new ERR_INVALID_FILE_URL_PATH(
          'must not include encoded \\ or / characters'
        );
      }
    }
  }
  pathname = pathname.replace(forwardSlashRegEx, '\\');
  pathname = decodeURIComponent(pathname);
  if (hostname !== '') {
    // If hostname is set, then we have a UNC path
    // Pass the hostname through domainToUnicode just in case
    // it is an IDN using punycode encoding. We do not need to worry
    // about percent encoding because the URL parser will have
    // already taken care of that for us. Note that this only
    // causes IDNs with an appropriate `xn--` prefix to be decoded.
    return `\\\\${domainToUnicode(hostname)}${pathname}`;
  } else {
    // Otherwise, it's a local path that requires a drive letter
    const letter = pathname.codePointAt(1) | 0x20;
    const sep = pathname[2];
    if (letter < CHAR_LOWERCASE_A || letter > CHAR_LOWERCASE_Z ||   // a..z A..Z
        (sep !== ':')) {
      throw new ERR_INVALID_FILE_URL_PATH('must be absolute');
    }
    return pathname.slice(1);
  }
}

function getPathFromURLPosix(url) {
  if (url.hostname !== '') {
    throw new ERR_INVALID_FILE_URL_HOST(platform);
  }
  const pathname = url.pathname;
  for (let n = 0; n < pathname.length; n++) {
    if (pathname[n] === '%') {
      const third = pathname.codePointAt(n + 2) | 0x20;
      if (pathname[n + 1] === '2' && third === 102) {
        throw new ERR_INVALID_FILE_URL_PATH(
          'must not include encoded / characters'
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
function pathToFileURL(filepath) {
  let resolved = path.resolve(filepath);
  // path.resolve strips trailing slashes so we must add them back
  const filePathLast = filepath.charCodeAt(filepath.length - 1);
  if ((filePathLast === CHAR_FORWARD_SLASH ||
       (isWindows && filePathLast === CHAR_BACKWARD_SLASH)) &&
      resolved[resolved.length - 1] !== path.sep)
    resolved += '/';
  const outURL = new URL('file://');
  if (resolved.includes('%'))
    resolved = resolved.replace(percentRegEx, '%25');
  // In posix, "/" is a valid character in paths
  if (!isWindows && resolved.includes('\\'))
    resolved = resolved.replace(backslashRegEx, '%5C');
  if (resolved.includes('\n'))
    resolved = resolved.replace(newlineRegEx, '%0A');
  if (resolved.includes('\r'))
    resolved = resolved.replace(carriageReturnRegEx, '%0D');
  if (resolved.includes('\t'))
    resolved = resolved.replace(tabRegEx, '%09');
  outURL.pathname = resolved;
  return outURL;
}

function isURLInstance(fileURLOrPath) {
  return fileURLOrPath != null && fileURLOrPath[searchParams] &&
    fileURLOrPath[searchParams][searchParams];
}

function toPathIfFileURL(fileURLOrPath) {
  if (!isURLInstance(fileURLOrPath))
    return fileURLOrPath;
  return fileURLToPath(fileURLOrPath);
}

function constructUrl(flags, protocol, username, password,
                      host, port, path, query, fragment) {
  const ctx = new URLContext();
  ctx.flags = flags;
  ctx.scheme = protocol;
  ctx.username = (flags & URL_FLAGS_HAS_USERNAME) !== 0 ? username : '';
  ctx.password = (flags & URL_FLAGS_HAS_PASSWORD) !== 0 ? password : '';
  ctx.port = port;
  ctx.path = (flags & URL_FLAGS_HAS_PATH) !== 0 ? path : [];
  ctx.query = query;
  ctx.fragment = fragment;
  ctx.host = host;

  const url = ObjectCreate(URL.prototype);
  url[context] = ctx;
  const params = new URLSearchParams();
  url[searchParams] = params;
  params[context] = url;
  initSearchParams(params, query);
  return url;
}
setURLConstructor(constructUrl);

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
  urlToOptions,
  formatSymbol: kFormat,
  searchParamsSymbol: searchParams,
  encodeStr
};
