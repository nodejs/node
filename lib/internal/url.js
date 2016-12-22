'use strict';

function getPunycode() {
  try {
    return process.binding('icu');
  } catch (err) {
    return require('punycode');
  }
}
const punycode = getPunycode();
const binding = process.binding('url');
const context = Symbol('context');
const cannotBeBase = Symbol('cannot-be-base');
const special = Symbol('special');
const searchParams = Symbol('query');
const querystring = require('querystring');

const kScheme = Symbol('scheme');
const kHost = Symbol('host');
const kPort = Symbol('port');
const kDomain = Symbol('domain');

// https://tc39.github.io/ecma262/#sec-%iteratorprototype%-object
const IteratorPrototype = Object.getPrototypeOf(
  Object.getPrototypeOf([][Symbol.iterator]())
);

function StorageObject() {}
StorageObject.prototype = Object.create(null);

class OpaqueOrigin {
  toString() {
    return 'null';
  }

  get effectiveDomain() {
    return this;
  }
}

class TupleOrigin {
  constructor(scheme, host, port, domain) {
    this[kScheme] = scheme;
    this[kHost] = host;
    this[kPort] = port;
    this[kDomain] = domain;
  }

  get scheme() {
    return this[kScheme];
  }

  get host() {
    return this[kHost];
  }

  get port() {
    return this[kPort];
  }

  get domain() {
    return this[kDomain];
  }

  get effectiveDomain() {
    return this[kDomain] || this[kHost];
  }

  toString(unicode = false) {
    var result = this.scheme;
    result += '://';
    result += unicode ? URL.domainToUnicode(this.host) : this.host;
    if (this.port !== undefined && this.port !== null)
      result += `:${this.port}`;
    return result;
  }

  inspect() {
    return `TupleOrigin {
      scheme: ${this.scheme},
      host: ${this.host},
      port: ${this.port},
      domain: ${this.domain}
    }`;
  }
}

// Reused by URL constructor and URL#href setter.
function parse(url, input, base) {
  input = String(input);
  const base_context = base ? base[context] : undefined;
  url[context] = new StorageObject();
  binding.parse(input.trim(), -1, base_context, undefined,
    (flags, protocol, username, password,
      host, port, path, query, fragment) => {
      if (flags & binding.URL_FLAGS_FAILED)
        throw new TypeError('Invalid URL');
      url[context].flags = flags;
      url[context].scheme = protocol;
      url[context].username = username;
      url[context].password = password;
      url[context].port = port;
      url[context].path = path;
      url[context].query = query;
      url[context].fragment = fragment;
      url[context].host = host;
      if (url[searchParams]) {  // invoked from href setter
        initSearchParams(url[searchParams], query);
      } else {
        url[searchParams] = new URLSearchParams(query);
      }
      url[searchParams][context] = url;
    });
}

class URL {
  constructor(input, base) {
    if (base !== undefined && !(base instanceof URL))
      base = new URL(String(base));
    parse(this, input, base);
  }

  get [special]() {
    return (this[context].flags & binding.URL_FLAGS_SPECIAL) != 0;
  }

  get [cannotBeBase]() {
    return (this[context].flags & binding.URL_FLAGS_CANNOT_BE_BASE) != 0;
  }

  inspect(depth, opts) {
    var ret = 'URL {\n';
    ret += `  href: ${this.href}\n`;
    if (this[context].scheme !== undefined)
      ret += `  protocol: ${this.protocol}\n`;
    if (this[context].username !== undefined)
      ret += `  username: ${this.username}\n`;
    if (this[context].password !== undefined) {
      const pwd = opts.showHidden ? this[context].password : '--------';
      ret += `  password: ${pwd}\n`;
    }
    if (this[context].host !== undefined)
      ret += `  hostname: ${this.hostname}\n`;
    if (this[context].port !== undefined)
      ret += `  port: ${this.port}\n`;
    if (this[context].path !== undefined)
      ret += `  pathname: ${this.pathname}\n`;
    if (this[context].query !== undefined)
      ret += `  search: ${this.search}\n`;
    if (this[context].fragment !== undefined)
      ret += `  hash: ${this.hash}\n`;
    if (opts.showHidden) {
      ret += `  cannot-be-base: ${this[cannotBeBase]}\n`;
      ret += `  special: ${this[special]}\n`;
    }
    ret += '}';
    return ret;
  }
}

Object.defineProperties(URL.prototype, {
  toString: {
    // https://heycam.github.io/webidl/#es-stringifier
    writable: true,
    enumerable: true,
    configurable: true,
    // eslint-disable-next-line func-name-matching
    value: function toString(options) {
      options = options || {};
      const fragment =
        options.fragment !== undefined ?
          !!options.fragment : true;
      const unicode = !!options.unicode;
      var ret;
      if (this.protocol)
        ret = this.protocol;
      if (this[context].host !== undefined) {
        ret += '//';
        const has_username = typeof this[context].username === 'string';
        const has_password = typeof this[context].password === 'string';
        if (has_username || has_password) {
          if (has_username)
            ret += this[context].username;
          if (has_password)
            ret += `:${this[context].password}`;
          ret += '@';
        }
        if (unicode) {
          ret += punycode.toUnicode(this.hostname);
          if (this.port !== undefined)
            ret += `:${this.port}`;
        } else {
          ret += this.host;
        }
      } else if (this[context].scheme === 'file:') {
        ret += '//';
      }
      if (this.pathname)
        ret += this.pathname;
      if (typeof this[context].query === 'string')
        ret += `?${this[context].query}`;
      if (fragment & typeof this[context].fragment === 'string')
        ret += `#${this[context].fragment}`;
      return ret;
    }
  },
  href: {
    enumerable: true,
    configurable: true,
    get() {
      return this.toString();
    },
    set(input) {
      parse(this, input);
    }
  },
  origin: {  // readonly
    enumerable: true,
    configurable: true,
    get() {
      return originFor(this).toString(true);
    }
  },
  protocol: {
    enumerable: true,
    configurable: true,
    get() {
      return this[context].scheme;
    },
    set(scheme) {
      scheme = String(scheme);
      if (scheme.length === 0)
        return;
      binding.parse(scheme,
                    binding.kSchemeStart,
                    null,
                    this[context],
        (flags, protocol, username, password,
        host, port, path, query, fragment) => {
          if (flags & binding.URL_FLAGS_FAILED)
            return;
          const newIsSpecial = (flags & binding.URL_FLAGS_SPECIAL) != 0;
          if ((this[special] && !newIsSpecial) ||
              (!this[special] && newIsSpecial) ||
              (newIsSpecial && !this[special] &&
              this[context].host === undefined)) {
            return;
          }
          if (newIsSpecial) {
            this[context].flags |= binding.URL_FLAGS_SPECIAL;
          } else {
            this[context].flags &= ~binding.URL_FLAGS_SPECIAL;
          }
          if (protocol) {
            this[context].scheme = protocol;
            this[context].flags |= binding.URL_FLAGS_HAS_SCHEME;
          } else {
            this[context].flags &= ~binding.URL_FLAGS_HAS_SCHEME;
          }
        });
    }
  },
  username: {
    enumerable: true,
    configurable: true,
    get() {
      return this[context].username || '';
    },
    set(username) {
      username = String(username);
      if (!this.hostname)
        return;
      if (!username) {
        this[context].username = null;
        this[context].flags &= ~binding.URL_FLAGS_HAS_USERNAME;
        return;
      }
      this[context].username = binding.encodeAuth(username);
      this[context].flags |= binding.URL_FLAGS_HAS_USERNAME;
    }
  },
  password: {
    enumerable: true,
    configurable: true,
    get() {
      return this[context].password || '';
    },
    set(password) {
      password = String(password);
      if (!this.hostname)
        return;
      if (!password) {
        this[context].password = null;
        this[context].flags &= ~binding.URL_FLAGS_HAS_PASSWORD;
        return;
      }
      this[context].password = binding.encodeAuth(password);
      this[context].flags |= binding.URL_FLAGS_HAS_PASSWORD;
    }
  },
  host: {
    enumerable: true,
    configurable: true,
    get() {
      var ret = this[context].host || '';
      if (this[context].port !== undefined)
        ret += `:${this[context].port}`;
      return ret;
    },
    set(host) {
      host = String(host);
      if (this[cannotBeBase] ||
          (this[special] && host.length === 0)) {
        // Cannot set the host if cannot-be-base is set or
        // scheme is special and host length is zero
        return;
      }
      if (!host) {
        this[context].host = null;
        this[context].flags &= ~binding.URL_FLAGS_HAS_HOST;
        return;
      }
      binding.parse(host, binding.kHost, null, this[context],
        (flags, protocol, username, password,
        host, port, path, query, fragment) => {
          if (flags & binding.URL_FLAGS_FAILED)
            return;
          if (host) {
            this[context].host = host;
            this[context].flags |= binding.URL_FLAGS_HAS_HOST;
          } else {
            this[context].flags &= ~binding.URL_FLAGS_HAS_HOST;
          }
          if (port !== undefined)
            this[context].port = port;
        });
    }
  },
  hostname: {
    enumerable: true,
    configurable: true,
    get() {
      return this[context].host || '';
    },
    set(host) {
      host = String(host);
      if (this[cannotBeBase] ||
          (this[special] && host.length === 0)) {
        // Cannot set the host if cannot-be-base is set or
        // scheme is special and host length is zero
        return;
      }
      if (!host) {
        this[context].host = null;
        this[context].flags &= ~binding.URL_FLAGS_HAS_HOST;
        return;
      }
      binding.parse(host,
                    binding.kHostname,
                    null,
                    this[context],
        (flags, protocol, username, password,
        host, port, path, query, fragment) => {
          if (flags & binding.URL_FLAGS_FAILED)
            return;
          if (host) {
            this[context].host = host;
            this[context].flags |= binding.URL_FLAGS_HAS_HOST;
          } else {
            this[context].flags &= ~binding.URL_FLAGS_HAS_HOST;
          }
        });
    }
  },
  port: {
    enumerable: true,
    configurable: true,
    get() {
      const port = this[context].port;
      return port === undefined ? '' : String(port);
    },
    set(port) {
      if (!this[context].host || this[cannotBeBase] ||
          this.protocol === 'file:')
        return;
      port = String(port);
      if (port === '') {
        // Currently, if port number is empty, left unchanged.
        // TODO(jasnell): This might be changing in the spec
        return;
      }
      binding.parse(port, binding.kPort, null, this[context],
        (flags, protocol, username, password,
        host, port, path, query, fragment) => {
          if (flags & binding.URL_FLAGS_FAILED)
            return;
          this[context].port = port;
        });
    }
  },
  pathname: {
    enumerable: true,
    configurable: true,
    get() {
      if (this[cannotBeBase])
        return this[context].path[0];
      return this[context].path !== undefined ?
        `/${this[context].path.join('/')}` : '';
    },
    set(path) {
      if (this[cannotBeBase])
        return;
      path = String(path);
      binding.parse(path,
                    binding.kPathStart,
                    null,
                    this[context],
        (flags, protocol, username, password,
        host, port, path, query, fragment) => {
          if (flags & binding.URL_FLAGS_FAILED)
            return;
          if (path) {
            this[context].path = path;
            this[context].flags |= binding.URL_FLAGS_HAS_PATH;
          } else {
            this[context].flags &= ~binding.URL_FLAGS_HAS_PATH;
          }
        });
    }
  },
  search: {
    enumerable: true,
    configurable: true,
    get() {
      return !this[context].query ? '' : `?${this[context].query}`;
    },
    set(search) {
      search = String(search);
      if (search[0] === '?') search = search.slice(1);
      if (!search) {
        this[context].query = null;
        this[context].flags &= ~binding.URL_FLAGS_HAS_QUERY;
        this[searchParams][searchParams] = {};
        return;
      }
      this[context].query = '';
      binding.parse(search,
                    binding.kQuery,
                    null,
                    this[context],
        (flags, protocol, username, password,
        host, port, path, query, fragment) => {
          if (flags & binding.URL_FLAGS_FAILED)
            return;
          if (query) {
            this[context].query = query;
            this[context].flags |= binding.URL_FLAGS_HAS_QUERY;
          } else {
            this[context].flags &= ~binding.URL_FLAGS_HAS_QUERY;
          }
        });
      this[searchParams][searchParams] = querystring.parse(search);
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
      return !this[context].fragment ? '' : `#${this[context].fragment}`;
    },
    set(hash) {
      hash = String(hash);
      if (this.protocol === 'javascript:')
        return;
      if (!hash) {
        this[context].fragment = null;
        this[context].flags &= ~binding.URL_FLAGS_HAS_FRAGMENT;
        return;
      }
      if (hash[0] === '#') hash = hash.slice(1);
      this[context].fragment = '';
      binding.parse(hash,
                    binding.kFragment,
                    null,
                    this[context],
        (flags, protocol, username, password,
        host, port, path, query, fragment) => {
          if (flags & binding.URL_FLAGS_FAILED)
            return;
          if (fragment) {
            this[context].fragment = fragment;
            this[context].flags |= binding.URL_FLAGS_HAS_FRAGMENT;
          } else {
            this[context].flags &= ~binding.URL_FLAGS_HAS_FRAGMENT;
          }
        });
    }
  }
});

var hexTable = new Array(256);
for (var i = 0; i < 256; ++i)
  hexTable[i] = '%' + ((i < 16 ? '0' : '') + i.toString(16)).toUpperCase();
function encodeAuth(str) {
  // faster encodeURIComponent alternative for encoding auth uri components
  var out = '';
  var lastPos = 0;
  for (var i = 0; i < str.length; ++i) {
    var c = str.charCodeAt(i);

    // These characters do not need escaping:
    // ! - . _ ~
    // ' ( ) * :
    // digits
    // alpha (uppercase)
    // alpha (lowercase)
    if (c === 0x21 || c === 0x2D || c === 0x2E || c === 0x5F || c === 0x7E ||
        (c >= 0x27 && c <= 0x2A) ||
        (c >= 0x30 && c <= 0x3A) ||
        (c >= 0x41 && c <= 0x5A) ||
        (c >= 0x61 && c <= 0x7A)) {
      continue;
    }

    if (i - lastPos > 0)
      out += str.slice(lastPos, i);

    lastPos = i + 1;

    // Other ASCII characters
    if (c < 0x80) {
      out += hexTable[c];
      continue;
    }

    // Multi-byte characters ...
    if (c < 0x800) {
      out += hexTable[0xC0 | (c >> 6)] + hexTable[0x80 | (c & 0x3F)];
      continue;
    }
    if (c < 0xD800 || c >= 0xE000) {
      out += hexTable[0xE0 | (c >> 12)] +
             hexTable[0x80 | ((c >> 6) & 0x3F)] +
             hexTable[0x80 | (c & 0x3F)];
      continue;
    }
    // Surrogate pair
    ++i;
    var c2;
    if (i < str.length)
      c2 = str.charCodeAt(i) & 0x3FF;
    else
      c2 = 0;
    c = 0x10000 + (((c & 0x3FF) << 10) | c2);
    out += hexTable[0xF0 | (c >> 18)] +
           hexTable[0x80 | ((c >> 12) & 0x3F)] +
           hexTable[0x80 | ((c >> 6) & 0x3F)] +
           hexTable[0x80 | (c & 0x3F)];
  }
  if (lastPos === 0)
    return str;
  if (lastPos < str.length)
    return out + str.slice(lastPos);
  return out;
}

function update(url, params) {
  if (!url)
    return;

  url[context].query = params.toString();
}

function getSearchParamPairs(target) {
  const obj = target[searchParams];
  const keys = Object.keys(obj);
  const values = [];
  for (var i = 0; i < keys.length; i++) {
    const name = keys[i];
    const value = obj[name];
    if (Array.isArray(value)) {
      for (const item of value)
        values.push([name, item]);
    } else {
      values.push([name, value]);
    }
  }
  return values;
}

// Reused by the URL parse function invoked by
// the href setter, and the URLSearchParams constructor
function initSearchParams(url, init) {
  url[searchParams] = querystring.parse(init);
}

class URLSearchParams {
  constructor(init = '') {
    if (init instanceof URLSearchParams) {
      const childParams = init[searchParams];
      this[searchParams] = Object.assign(Object.create(null), childParams);
    } else {
      init = String(init);
      if (init[0] === '?') init = init.slice(1);
      initSearchParams(this, init);
    }

    // "associated url object"
    this[context] = null;

    // Class string for an instance of URLSearchParams. This is different from
    // the class string of the prototype object (set below).
    Object.defineProperty(this, Symbol.toStringTag, {
      value: 'URLSearchParams',
      writable: false,
      enumerable: false,
      configurable: true
    });
  }

  append(name, value) {
    if (!this || !(this instanceof URLSearchParams)) {
      throw new TypeError('Value of `this` is not a URLSearchParams');
    }
    if (arguments.length < 2) {
      throw new TypeError(
        'Both `name` and `value` arguments need to be specified');
    }

    const obj = this[searchParams];
    name = String(name);
    value = String(value);
    var existing = obj[name];
    if (existing === undefined) {
      obj[name] = value;
    } else if (Array.isArray(existing)) {
      existing.push(value);
    } else {
      obj[name] = [existing, value];
    }
    update(this[context], this);
  }

  delete(name) {
    if (!this || !(this instanceof URLSearchParams)) {
      throw new TypeError('Value of `this` is not a URLSearchParams');
    }
    if (arguments.length < 1) {
      throw new TypeError('The `name` argument needs to be specified');
    }

    const obj = this[searchParams];
    name = String(name);
    delete obj[name];
    update(this[context], this);
  }

  set(name, value) {
    if (!this || !(this instanceof URLSearchParams)) {
      throw new TypeError('Value of `this` is not a URLSearchParams');
    }
    if (arguments.length < 2) {
      throw new TypeError(
        'Both `name` and `value` arguments need to be specified');
    }

    const obj = this[searchParams];
    name = String(name);
    value = String(value);
    obj[name] = value;
    update(this[context], this);
  }

  get(name) {
    if (!this || !(this instanceof URLSearchParams)) {
      throw new TypeError('Value of `this` is not a URLSearchParams');
    }
    if (arguments.length < 1) {
      throw new TypeError('The `name` argument needs to be specified');
    }

    const obj = this[searchParams];
    name = String(name);
    var value = obj[name];
    return value === undefined ? null : Array.isArray(value) ? value[0] : value;
  }

  getAll(name) {
    if (!this || !(this instanceof URLSearchParams)) {
      throw new TypeError('Value of `this` is not a URLSearchParams');
    }
    if (arguments.length < 1) {
      throw new TypeError('The `name` argument needs to be specified');
    }

    const obj = this[searchParams];
    name = String(name);
    var value = obj[name];
    return value === undefined ? [] : Array.isArray(value) ? value : [value];
  }

  has(name) {
    if (!this || !(this instanceof URLSearchParams)) {
      throw new TypeError('Value of `this` is not a URLSearchParams');
    }
    if (arguments.length < 1) {
      throw new TypeError('The `name` argument needs to be specified');
    }

    const obj = this[searchParams];
    name = String(name);
    return name in obj;
  }

  // https://heycam.github.io/webidl/#es-iterators
  // Define entries here rather than [Symbol.iterator] as the function name
  // must be set to `entries`.
  entries() {
    if (!this || !(this instanceof URLSearchParams)) {
      throw new TypeError('Value of `this` is not a URLSearchParams');
    }

    return createSearchParamsIterator(this, 'key+value');
  }

  forEach(callback, thisArg = undefined) {
    if (!this || !(this instanceof URLSearchParams)) {
      throw new TypeError('Value of `this` is not a URLSearchParams');
    }
    if (arguments.length < 1) {
      throw new TypeError('The `callback` argument needs to be specified');
    }

    let pairs = getSearchParamPairs(this);

    var i = 0;
    while (i < pairs.length) {
      const [key, value] = pairs[i];
      callback.call(thisArg, value, key, this);
      pairs = getSearchParamPairs(this);
      i++;
    }
  }

  // https://heycam.github.io/webidl/#es-iterable
  keys() {
    if (!this || !(this instanceof URLSearchParams)) {
      throw new TypeError('Value of `this` is not a URLSearchParams');
    }

    return createSearchParamsIterator(this, 'key');
  }

  values() {
    if (!this || !(this instanceof URLSearchParams)) {
      throw new TypeError('Value of `this` is not a URLSearchParams');
    }

    return createSearchParamsIterator(this, 'value');
  }

  // https://url.spec.whatwg.org/#urlsearchparams-stringification-behavior
  toString() {
    if (!this || !(this instanceof URLSearchParams)) {
      throw new TypeError('Value of `this` is not a URLSearchParams');
    }

    return querystring.stringify(this[searchParams]);
  }
}
// https://heycam.github.io/webidl/#es-iterable-entries
URLSearchParams.prototype[Symbol.iterator] = URLSearchParams.prototype.entries;
Object.defineProperty(URLSearchParams.prototype, Symbol.toStringTag, {
  value: 'URLSearchParamsPrototype',
  writable: false,
  enumerable: false,
  configurable: true
});

// https://heycam.github.io/webidl/#dfn-default-iterator-object
function createSearchParamsIterator(target, kind) {
  const iterator = Object.create(URLSearchParamsIteratorPrototype);
  iterator[context] = {
    target,
    kind,
    index: 0
  };
  return iterator;
}

// https://heycam.github.io/webidl/#dfn-iterator-prototype-object
const URLSearchParamsIteratorPrototype = Object.setPrototypeOf({
  next() {
    if (!this ||
        Object.getPrototypeOf(this) !== URLSearchParamsIteratorPrototype) {
      throw new TypeError('Value of `this` is not a URLSearchParamsIterator');
    }

    const {
      target,
      kind,
      index
    } = this[context];
    const values = getSearchParamPairs(target);
    const len = values.length;
    if (index >= len) {
      return {
        value: undefined,
        done: true
      };
    }

    const pair = values[index];
    this[context].index = index + 1;

    let result;
    if (kind === 'key') {
      result = pair[0];
    } else if (kind === 'value') {
      result = pair[1];
    } else {
      result = pair;
    }

    return {
      value: result,
      done: false
    };
  }
}, IteratorPrototype);

// Unlike interface and its prototype object, both default iterator object and
// iterator prototype object of an interface have the same class string.
Object.defineProperty(URLSearchParamsIteratorPrototype, Symbol.toStringTag, {
  value: 'URLSearchParamsIterator',
  writable: false,
  enumerable: false,
  configurable: true
});

function originFor(url, base) {
  if (!(url instanceof URL))
    url = new URL(url, base);
  var origin;
  const protocol = url.protocol;
  switch (protocol) {
    case 'blob:':
      if (url[context].path && url[context].path.length > 0) {
        try {
          return (new URL(url[context].path[0])).origin;
        } catch (err) {
          // fall through... do nothing
        }
      }
      origin = new OpaqueOrigin();
      break;
    case 'ftp:':
    case 'gopher:':
    case 'http:':
    case 'https:':
    case 'ws:':
    case 'wss:':
    case 'file':
      origin = new TupleOrigin(protocol.slice(0, -1),
                               url[context].host,
                               url[context].port,
                               null);
      break;
    default:
      origin = new OpaqueOrigin();
  }
  return origin;
}

URL.originFor = originFor;
URL.domainToASCII = function(domain) {
  return binding.domainToASCII(String(domain));
};
URL.domainToUnicode = function(domain) {
  return binding.domainToUnicode(String(domain));
};

exports.URL = URL;
exports.encodeAuth = encodeAuth;
