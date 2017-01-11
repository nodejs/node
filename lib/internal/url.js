'use strict';

const util = require('util');
const binding = process.binding('url');
const context = Symbol('context');
const cannotBeBase = Symbol('cannot-be-base');
const special = Symbol('special');
const searchParams = Symbol('query');
const querystring = require('querystring');
const os = require('os');

const isWindows = process.platform === 'win32';

const kScheme = Symbol('scheme');
const kHost = Symbol('host');
const kPort = Symbol('port');
const kDomain = Symbol('domain');
const kFormat = Symbol('format');

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

  // https://url.spec.whatwg.org/#dom-url-origin
  toString(unicode = true) {
    var result = this[kScheme];
    result += '://';
    result += unicode ? domainToUnicode(this[kHost]) : this[kHost];
    if (this[kPort] !== undefined && this[kPort] !== null)
      result += `:${this[kPort]}`;
    return result;
  }

  [util.inspect.custom]() {
    return `TupleOrigin {
      scheme: ${this[kScheme]},
      host: ${this[kHost]},
      port: ${this[kPort]},
      domain: ${this[kDomain]}
    }`;
  }
}

function onParseComplete(flags, protocol, username, password,
                         host, port, path, query, fragment) {
  if (flags & binding.URL_FLAGS_FAILED)
    throw new TypeError('Invalid URL');
  var ctx = this[context];
  ctx.flags = flags;
  ctx.scheme = protocol;
  ctx.username = username;
  ctx.password = password;
  ctx.port = port;
  ctx.path = path;
  ctx.query = query;
  ctx.fragment = fragment;
  ctx.host = host;
  if (this[searchParams]) {  // invoked from href setter
    initSearchParams(this[searchParams], query);
  } else {
    this[searchParams] = new URLSearchParams(query);
  }
  this[searchParams][context] = this;
}

// Reused by URL constructor and URL#href setter.
function parse(url, input, base) {
  input = String(input);
  const base_context = base ? base[context] : undefined;
  url[context] = new StorageObject();
  binding.parse(input.trim(), -1,
                base_context, undefined,
                onParseComplete.bind(url));
}

function onParseProtocolComplete(flags, protocol, username, password,
                                 host, port, path, query, fragment) {
  if (flags & binding.URL_FLAGS_FAILED)
    return;
  const newIsSpecial = (flags & binding.URL_FLAGS_SPECIAL) !== 0;
  const s = this[special];
  const ctx = this[context];
  if ((s && !newIsSpecial) || (!s && newIsSpecial)) {
    return;
  }
  if (newIsSpecial) {
    ctx.flags |= binding.URL_FLAGS_SPECIAL;
  } else {
    ctx.flags &= ~binding.URL_FLAGS_SPECIAL;
  }
  if (protocol) {
    ctx.scheme = protocol;
    ctx.flags |= binding.URL_FLAGS_HAS_SCHEME;
  } else {
    ctx.flags &= ~binding.URL_FLAGS_HAS_SCHEME;
  }
}

function onParseHostComplete(flags, protocol, username, password,
                             host, port, path, query, fragment) {
  if (flags & binding.URL_FLAGS_FAILED)
    return;
  const ctx = this[context];
  if (host) {
    ctx.host = host;
    ctx.flags |= binding.URL_FLAGS_HAS_HOST;
  } else {
    ctx.flags &= ~binding.URL_FLAGS_HAS_HOST;
  }
  if (port !== undefined)
    ctx.port = port;
}

function onParseHostnameComplete(flags, protocol, username, password,
                                 host, port, path, query, fragment) {
  if (flags & binding.URL_FLAGS_FAILED)
    return;
  const ctx = this[context];
  if (host) {
    ctx.host = host;
    ctx.flags |= binding.URL_FLAGS_HAS_HOST;
  } else {
    ctx.flags &= ~binding.URL_FLAGS_HAS_HOST;
  }
}

function onParsePortComplete(flags, protocol, username, password,
                             host, port, path, query, fragment) {
  if (flags & binding.URL_FLAGS_FAILED)
    return;
  this[context].port = port;
}

function onParsePathComplete(flags, protocol, username, password,
                             host, port, path, query, fragment) {
  if (flags & binding.URL_FLAGS_FAILED)
    return;
  const ctx = this[context];
  if (path) {
    ctx.path = path;
    ctx.flags |= binding.URL_FLAGS_HAS_PATH;
  } else {
    ctx.flags &= ~binding.URL_FLAGS_HAS_PATH;
  }
}

function onParseSearchComplete(flags, protocol, username, password,
                               host, port, path, query, fragment) {
  if (flags & binding.URL_FLAGS_FAILED)
    return;
  const ctx = this[context];
  ctx.query = query;
}

function onParseHashComplete(flags, protocol, username, password,
                             host, port, path, query, fragment) {
  if (flags & binding.URL_FLAGS_FAILED)
    return;
  const ctx = this[context];
  if (fragment) {
    ctx.fragment = fragment;
    ctx.flags |= binding.URL_FLAGS_HAS_FRAGMENT;
  } else {
    ctx.flags &= ~binding.URL_FLAGS_HAS_FRAGMENT;
  }
}

class URL {
  constructor(input, base) {
    if (base !== undefined && !(base instanceof URL))
      base = new URL(String(base));
    parse(this, input, base);
  }

  get [special]() {
    return (this[context].flags & binding.URL_FLAGS_SPECIAL) !== 0;
  }

  get [cannotBeBase]() {
    return (this[context].flags & binding.URL_FLAGS_CANNOT_BE_BASE) !== 0;
  }

  [util.inspect.custom](depth, opts) {
    const ctx = this[context];
    var ret = 'URL {\n';
    ret += `  href: ${this.href}\n`;
    if (ctx.scheme !== undefined)
      ret += `  protocol: ${this.protocol}\n`;
    if (ctx.username !== undefined)
      ret += `  username: ${this.username}\n`;
    if (ctx.password !== undefined) {
      const pwd = opts.showHidden ? ctx.password : '--------';
      ret += `  password: ${pwd}\n`;
    }
    if (ctx.host !== undefined)
      ret += `  hostname: ${this.hostname}\n`;
    if (ctx.port !== undefined)
      ret += `  port: ${this.port}\n`;
    if (ctx.path !== undefined)
      ret += `  pathname: ${this.pathname}\n`;
    if (ctx.query !== undefined)
      ret += `  search: ${this.search}\n`;
    if (ctx.fragment !== undefined)
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
  [kFormat]: {
    enumerable: false,
    configurable: false,
    // eslint-disable-next-line func-name-matching
    value: function format(options) {
      if (options && typeof options !== 'object')
        throw new TypeError('options must be an object');
      options = Object.assign({
        fragment: true,
        unicode: false,
        search: true,
        auth: true
      }, options);
      const ctx = this[context];
      var ret;
      if (this.protocol)
        ret = this.protocol;
      if (ctx.host !== undefined) {
        ret += '//';
        const has_username = typeof ctx.username === 'string';
        const has_password = typeof ctx.password === 'string' &&
                             ctx.password !== '';
        if (options.auth && (has_username || has_password)) {
          if (has_username)
            ret += ctx.username;
          if (has_password)
            ret += `:${ctx.password}`;
          ret += '@';
        }
        ret += options.unicode ?
          domainToUnicode(this.host) : this.host;
      } else if (ctx.scheme === 'file:') {
        ret += '//';
      }
      if (this.pathname)
        ret += this.pathname;
      if (options.search && typeof ctx.query === 'string')
        ret += `?${ctx.query}`;
      if (options.fragment && typeof ctx.fragment === 'string')
        ret += `#${ctx.fragment}`;
      return ret;
    }
  },
  [Symbol.toStringTag]: {
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
      parse(this, input);
    }
  },
  origin: {  // readonly
    enumerable: true,
    configurable: true,
    get() {
      return originFor(this).toString();
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
      binding.parse(scheme, binding.kSchemeStart, null, this[context],
                    onParseProtocolComplete.bind(this));
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
      const ctx = this[context];
      if (!username) {
        ctx.username = null;
        ctx.flags &= ~binding.URL_FLAGS_HAS_USERNAME;
        return;
      }
      ctx.username = binding.encodeAuth(username);
      ctx.flags |= binding.URL_FLAGS_HAS_USERNAME;
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
      const ctx = this[context];
      if (!password) {
        ctx.password = null;
        ctx.flags &= ~binding.URL_FLAGS_HAS_PASSWORD;
        return;
      }
      ctx.password = binding.encodeAuth(password);
      ctx.flags |= binding.URL_FLAGS_HAS_PASSWORD;
    }
  },
  host: {
    enumerable: true,
    configurable: true,
    get() {
      const ctx = this[context];
      var ret = ctx.host || '';
      if (ctx.port !== undefined)
        ret += `:${ctx.port}`;
      return ret;
    },
    set(host) {
      const ctx = this[context];
      host = String(host);
      if (this[cannotBeBase] ||
          (this[special] && host.length === 0)) {
        // Cannot set the host if cannot-be-base is set or
        // scheme is special and host length is zero
        return;
      }
      if (!host) {
        ctx.host = null;
        ctx.flags &= ~binding.URL_FLAGS_HAS_HOST;
        return;
      }
      binding.parse(host, binding.kHost, null, ctx,
                    onParseHostComplete.bind(this));
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
      host = String(host);
      if (this[cannotBeBase] ||
          (this[special] && host.length === 0)) {
        // Cannot set the host if cannot-be-base is set or
        // scheme is special and host length is zero
        return;
      }
      if (!host) {
        ctx.host = null;
        ctx.flags &= ~binding.URL_FLAGS_HAS_HOST;
        return;
      }
      binding.parse(host, binding.kHostname, null, ctx,
                    onParseHostnameComplete.bind(this));
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
      const ctx = this[context];
      if (!ctx.host || this[cannotBeBase] ||
          this.protocol === 'file:')
        return;
      port = String(port);
      if (port === '') {
        ctx.port = undefined;
        return;
      }
      binding.parse(port, binding.kPort, null, ctx,
                    onParsePortComplete.bind(this));
    }
  },
  pathname: {
    enumerable: true,
    configurable: true,
    get() {
      const ctx = this[context];
      if (this[cannotBeBase])
        return ctx.path[0];
      return ctx.path !== undefined ? `/${ctx.path.join('/')}` : '';
    },
    set(path) {
      if (this[cannotBeBase])
        return;
      binding.parse(String(path), binding.kPathStart, null, this[context],
                    onParsePathComplete.bind(this));
    }
  },
  search: {
    enumerable: true,
    configurable: true,
    get() {
      const ctx = this[context];
      return !ctx.query ? '' : `?${ctx.query}`;
    },
    set(search) {
      const ctx = this[context];
      search = String(search);
      if (!search) {
        ctx.query = null;
        ctx.flags &= ~binding.URL_FLAGS_HAS_QUERY;
      } else {
        if (search[0] === '?') search = search.slice(1);
        ctx.query = '';
        ctx.flags |= binding.URL_FLAGS_HAS_QUERY;
        if (search) {
          binding.parse(search, binding.kQuery, null, ctx,
                        onParseSearchComplete.bind(this));
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
      const ctx = this[context];
      return !ctx.fragment ? '' : `#${ctx.fragment}`;
    },
    set(hash) {
      const ctx = this[context];
      hash = String(hash);
      if (this.protocol === 'javascript:')
        return;
      if (!hash) {
        ctx.fragment = null;
        ctx.flags &= ~binding.URL_FLAGS_HAS_FRAGMENT;
        return;
      }
      if (hash[0] === '#') hash = hash.slice(1);
      ctx.fragment = '';
      binding.parse(hash, binding.kFragment, null, ctx,
                    onParseHashComplete.bind(this));
    }
  }
});

const hexTable = new Array(256);

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

  const ctx = url[context];
  const serializedParams = params.toString();
  if (serializedParams) {
    ctx.query = serializedParams;
    ctx.flags |= binding.URL_FLAGS_HAS_QUERY;
  } else {
    ctx.query = null;
    ctx.flags &= ~binding.URL_FLAGS_HAS_QUERY;
  }
}

function initSearchParams(url, init) {
  if (!init) {
    url[searchParams] = [];
    return;
  }
  url[searchParams] = getParamsFromObject(querystring.parse(init));
}

function getParamsFromObject(obj) {
  const keys = Object.keys(obj);
  const values = [];
  for (var i = 0; i < keys.length; i++) {
    const name = keys[i];
    const value = obj[name];
    if (Array.isArray(value)) {
      for (const item of value)
        values.push(name, item);
    } else {
      values.push(name, value);
    }
  }
  return values;
}

function getObjectFromParams(array) {
  const obj = new StorageObject();
  for (var i = 0; i < array.length; i += 2) {
    const name = array[i];
    const value = array[i + 1];
    if (obj[name]) {
      obj[name].push(value);
    } else {
      obj[name] = [value];
    }
  }
  return obj;
}

// Mainly to mitigate func-name-matching ESLint rule
function defineIDLClass(proto, classStr, obj) {
  // https://heycam.github.io/webidl/#dfn-class-string
  Object.defineProperty(proto, Symbol.toStringTag, {
    writable: false,
    enumerable: false,
    configurable: true,
    value: classStr
  });

  // https://heycam.github.io/webidl/#es-operations
  for (const key of Object.keys(obj)) {
    Object.defineProperty(proto, key, {
      writable: true,
      enumerable: true,
      configurable: true,
      value: obj[key]
    });
  }
  for (const key of Object.getOwnPropertySymbols(obj)) {
    Object.defineProperty(proto, key, {
      writable: true,
      enumerable: false,
      configurable: true,
      value: obj[key]
    });
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
    } else if (typeof init === 'object') {
      const method = init[Symbol.iterator];
      if (method === this[Symbol.iterator]) {
        // While the spec does not have this branch, we can use it as a
        // shortcut to avoid having to go through the costly generic iterator.
        const childParams = init[searchParams];
        this[searchParams] = childParams.slice();
      } else if (method !== null && method !== undefined) {
        if (typeof method !== 'function') {
          throw new TypeError('Query pairs must be iterable');
        }

        // sequence<sequence<USVString>>
        // Note: per spec we have to first exhaust the lists then process them
        const pairs = [];
        for (const pair of init) {
          if (typeof pair !== 'object' ||
              typeof pair[Symbol.iterator] !== 'function') {
            throw new TypeError('Each query pair must be iterable');
          }
          pairs.push(Array.from(pair));
        }

        this[searchParams] = [];
        for (const pair of pairs) {
          if (pair.length !== 2) {
            throw new TypeError('Each query pair must be a name/value tuple');
          }
          this[searchParams].push(String(pair[0]), String(pair[1]));
        }
      } else {
        // record<USVString, USVString>
        this[searchParams] = [];
        for (const key of Object.keys(init)) {
          const value = String(init[key]);
          this[searchParams].push(key, value);
        }
      }
    } else {
      // USVString
      init = String(init);
      if (init[0] === '?') init = init.slice(1);
      initSearchParams(this, init);
    }

    // "associated url object"
    this[context] = null;
  }

  [util.inspect.custom](recurseTimes, ctx) {
    if (!this || !(this instanceof URLSearchParams)) {
      throw new TypeError('Value of `this` is not a URLSearchParams');
    }

    const separator = ', ';
    const innerOpts = Object.assign({}, ctx);
    if (recurseTimes !== null) {
      innerOpts.depth = recurseTimes - 1;
    }
    const innerInspect = (v) => util.inspect(v, innerOpts);

    const list = this[searchParams];
    const output = [];
    for (var i = 0; i < list.length; i += 2)
      output.push(`${innerInspect(list[i])} => ${innerInspect(list[i + 1])}`);

    const colorRe = /\u001b\[\d\d?m/g;
    const length = output.reduce(
      (prev, cur) => prev + cur.replace(colorRe, '').length + separator.length,
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

defineIDLClass(URLSearchParams.prototype, 'URLSearchParams', {
  append(name, value) {
    if (!this || !(this instanceof URLSearchParams)) {
      throw new TypeError('Value of `this` is not a URLSearchParams');
    }
    if (arguments.length < 2) {
      throw new TypeError('"name" and "value" arguments must be specified');
    }

    name = String(name);
    value = String(value);
    this[searchParams].push(name, value);
    update(this[context], this);
  },

  delete(name) {
    if (!this || !(this instanceof URLSearchParams)) {
      throw new TypeError('Value of `this` is not a URLSearchParams');
    }
    if (arguments.length < 1) {
      throw new TypeError('"name" argument must be specified');
    }

    const list = this[searchParams];
    name = String(name);
    for (var i = 0; i < list.length;) {
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
    if (!this || !(this instanceof URLSearchParams)) {
      throw new TypeError('Value of `this` is not a URLSearchParams');
    }
    if (arguments.length < 1) {
      throw new TypeError('"name" argument must be specified');
    }

    const list = this[searchParams];
    name = String(name);
    for (var i = 0; i < list.length; i += 2) {
      if (list[i] === name) {
        return list[i + 1];
      }
    }
    return null;
  },

  getAll(name) {
    if (!this || !(this instanceof URLSearchParams)) {
      throw new TypeError('Value of `this` is not a URLSearchParams');
    }
    if (arguments.length < 1) {
      throw new TypeError('"name" argument must be specified');
    }

    const list = this[searchParams];
    const values = [];
    name = String(name);
    for (var i = 0; i < list.length; i += 2) {
      if (list[i] === name) {
        values.push(list[i + 1]);
      }
    }
    return values;
  },

  has(name) {
    if (!this || !(this instanceof URLSearchParams)) {
      throw new TypeError('Value of `this` is not a URLSearchParams');
    }
    if (arguments.length < 1) {
      throw new TypeError('"name" argument must be specified');
    }

    const list = this[searchParams];
    name = String(name);
    for (var i = 0; i < list.length; i += 2) {
      if (list[i] === name) {
        return true;
      }
    }
    return false;
  },

  set(name, value) {
    if (!this || !(this instanceof URLSearchParams)) {
      throw new TypeError('Value of `this` is not a URLSearchParams');
    }
    if (arguments.length < 2) {
      throw new TypeError('"name" and "value" arguments must be specified');
    }

    const list = this[searchParams];
    name = String(name);
    value = String(value);

    // If there are any name-value pairs whose name is `name`, in `list`, set
    // the value of the first such name-value pair to `value` and remove the
    // others.
    var found = false;
    for (var i = 0; i < list.length;) {
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

  // https://heycam.github.io/webidl/#es-iterators
  // Define entries here rather than [Symbol.iterator] as the function name
  // must be set to `entries`.
  entries() {
    if (!this || !(this instanceof URLSearchParams)) {
      throw new TypeError('Value of `this` is not a URLSearchParams');
    }

    return createSearchParamsIterator(this, 'key+value');
  },

  forEach(callback, thisArg = undefined) {
    if (!this || !(this instanceof URLSearchParams)) {
      throw new TypeError('Value of `this` is not a URLSearchParams');
    }
    if (typeof callback !== 'function') {
      throw new TypeError('"callback" argument must be a function');
    }

    let list = this[searchParams];

    var i = 0;
    while (i < list.length) {
      const key = list[i];
      const value = list[i + 1];
      callback.call(thisArg, value, key, this);
      // in case the URL object's `search` is updated
      list = this[searchParams];
      i += 2;
    }
  },

  // https://heycam.github.io/webidl/#es-iterable
  keys() {
    if (!this || !(this instanceof URLSearchParams)) {
      throw new TypeError('Value of `this` is not a URLSearchParams');
    }

    return createSearchParamsIterator(this, 'key');
  },

  values() {
    if (!this || !(this instanceof URLSearchParams)) {
      throw new TypeError('Value of `this` is not a URLSearchParams');
    }

    return createSearchParamsIterator(this, 'value');
  },

  // https://heycam.github.io/webidl/#es-stringifier
  // https://url.spec.whatwg.org/#urlsearchparams-stringification-behavior
  toString() {
    if (!this || !(this instanceof URLSearchParams)) {
      throw new TypeError('Value of `this` is not a URLSearchParams');
    }

    return querystring.stringify(getObjectFromParams(this[searchParams]));
  }
});

// https://heycam.github.io/webidl/#es-iterable-entries
Object.defineProperty(URLSearchParams.prototype, Symbol.iterator, {
  writable: true,
  configurable: true,
  value: URLSearchParams.prototype.entries
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
const URLSearchParamsIteratorPrototype = Object.create(IteratorPrototype);

defineIDLClass(URLSearchParamsIteratorPrototype, 'URLSearchParamsIterator', {
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
  [util.inspect.custom](recurseTimes, ctx) {
    const innerOpts = Object.assign({}, ctx);
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
    const breakLn = util.inspect(output, innerOpts).includes('\n');
    const outputStrs = output.map((p) => util.inspect(p, innerOpts));
    let outputStr;
    if (breakLn) {
      outputStr = `\n  ${outputStrs.join(',\n  ')}`;
    } else {
      outputStr = ` ${outputStrs.join(', ')}`;
    }
    return `${this[Symbol.toStringTag]} {${outputStr} }`;
  }
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

function domainToASCII(domain) {
  return binding.domainToASCII(String(domain));
}

function domainToUnicode(domain) {
  return binding.domainToUnicode(String(domain));
}

// Utility function that converts a URL object into an ordinary
// options object as expected by the http.request and https.request
// APIs.
function urlToOptions(url) {
  var options = {
    protocol: url.protocol,
    host: url.host,
    hostname: url.hostname,
    hash: url.hash,
    search: url.search,
    pathname: url.pathname,
    path: `${url.pathname}${url.search}`,
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

function getPathFromURLWin32(url) {
  var hostname = url.hostname;
  var pathname = url.pathname;
  for (var n = 0; n < pathname.length; n++) {
    if (pathname[n] === '%') {
      var third = pathname.codePointAt(n + 2) | 0x20;
      if ((pathname[n + 1] === '2' && third === 102) || // 2f 2F /
          (pathname[n + 1] === '5' && third === 99)) {  // 5c 5C \
        return new TypeError(
          'Path must not include encoded \\ or / characters');
      }
    }
  }
  pathname = decodeURIComponent(pathname);
  if (hostname !== '') {
    // If hostname is set, then we have a UNC path
    // Pass the hostname through domainToUnicode just in case
    // it is an IDN using punycode encoding. We do not need to worry
    // about percent encoding because the URL parser will have
    // already taken care of that for us. Note that this only
    // causes IDNs with an appropriate `xn--` prefix to be decoded.
    return `//${domainToUnicode(hostname)}${pathname}`;
  } else {
    // Otherwise, it's a local path that requires a drive letter
    var letter = pathname.codePointAt(1) | 0x20;
    var sep = pathname[2];
    if (letter < 97 || letter > 122 ||   // a..z A..Z
        (sep !== ':')) {
      return new TypeError('File URLs must specify absolute paths');
    }
    return pathname.slice(1);
  }
}

function getPathFromURLPosix(url) {
  if (url.hostname !== '') {
    return new TypeError(
      `File URLs on ${os.platform()} must use hostname 'localhost'` +
      ' or not specify any hostname');
  }
  var pathname = url.pathname;
  for (var n = 0; n < pathname.length; n++) {
    if (pathname[n] === '%') {
      var third = pathname.codePointAt(n + 2) | 0x20;
      if (pathname[n + 1] === '2' && third === 102) {
        return new TypeError('Path must not include encoded / characters');
      }
    }
  }
  return decodeURIComponent(pathname);
}

function getPathFromURL(path) {
  if (!(path instanceof URL))
    return path;
  if (path.protocol !== 'file:')
    return new TypeError('Only `file:` URLs are supported');
  return isWindows ? getPathFromURLWin32(path) : getPathFromURLPosix(path);
}

exports.getPathFromURL = getPathFromURL;
exports.URL = URL;
exports.URLSearchParams = URLSearchParams;
exports.domainToASCII = domainToASCII;
exports.domainToUnicode = domainToUnicode;
exports.encodeAuth = encodeAuth;
exports.urlToOptions = urlToOptions;
exports.formatSymbol = kFormat;
