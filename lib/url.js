'use strict';

const punycode = require('punycode');

exports.parse = urlParse;
exports.resolve = urlResolve;
exports.resolveObject = urlResolveObject;
exports.format = urlFormat;

exports.Url = Url;

function Url() {
  // For more efficient internal representation and laziness.
  // The non-underscore versions of these properties are accessor functions
  // defined on the prototype.
  this._protocol = null;
  this._port = -1;
  this._query = null;
  this._auth = null;
  this._hostname = null;
  this._host = null;
  this._pathname = null;
  this._hash = null;
  this._search = null;
  this._href = '';

  this._prependSlash = false;
  this._parsesQueryStrings = false;

  this.slashes = null;
}

// Reference: RFC 3986, RFC 1808, RFC 2396

const _protocolCharacters = makeAsciiTable([
  [0x61, 0x7A] /*a-z*/,
  [0x41, 0x5A] /*A-Z*/,
  0x2E /*'.'*/, 0x2B /*'+'*/, 0x2D /*'-'*/
]);

// RFC 2396: characters reserved for delimiting URLs.
// We actually just auto-escape these.
// RFC 2396: characters not allowed for various reasons.
// Allowed by RFCs, but cause of XSS attacks.  Always escape these.
const _autoEscape = [
  '<', '>', '\'', '`', ' ', '\r', '\n', '\t', '{', '}', '|', '\\', '^', '`', '"'
];

const _autoEscapeMap = new Array(128);

for (let i = 0, len = _autoEscapeMap.length; i < len; ++i) {
  _autoEscapeMap[i] = '';
}

for (let i = 0, len = _autoEscape.length; i < len; ++i) {
  let c = _autoEscape[i];
  let esc = encodeURIComponent(c);
  if (esc === c)
    esc = escape(c);
  _autoEscapeMap[c.charCodeAt(0)] = esc;
}

// Same as autoEscapeMap except \ is not escaped but is turned into /.
const _afterQueryAutoEscapeMap = _autoEscapeMap.slice();
_autoEscapeMap[0x5C /*'\'*/] = '/';

// Protocols that always contain a // bit.
const _slashProtocols = {
  'http': true,
  'https': true,
  'ftp': true,
  'gopher': true,
  'file': true,
  'http:': true,
  'https:': true,
  'ftp:': true,
  'gopher:': true,
  'file:': true
};

const _autoEscapeCharacters = makeAsciiTable(_autoEscape.map(function(v) {
  return v.charCodeAt(0);
}));

// Characters that are never ever allowed in a hostname.
// Note that any invalid chars are also handled, but these
// are the ones that are *expected* to be seen, so we fast-path them.
const _hostEndingCharacters = makeAsciiTable(
    ['#', '?', '/', '\\'].map(function(v) {
      return v.charCodeAt(0);
    }));
// If these characters end a host name, the path will not be prepended a /.
const _hostEndingCharactersNoPrependSlash = makeAsciiTable([
  '<', '>', '"', '`', ' ', '\r', '\n', '\t', '{', '}', '|', '^', '`', '\'', '%',
  ';'
].map(function(v) {
  return v.charCodeAt(0);
}));

const querystring = require('querystring');

const unserializablePropertyNames = ['_protocol', '_port', '_href', '_query',
                                     '_prependSlash', '_auth', '_hostname',
                                     '_pathname', '_hash', '_search',
                                     '_parsesQueryStrings', '_host'];
const noSerializePattern = new RegExp('^(?:' +
                                      unserializablePropertyNames.join('|') +
                                      ')$');

function urlParse(url, parseQueryString, slashesDenoteHost) {
  if (url instanceof Url) return url;

  var u = new Url();
  u.parse(url, parseQueryString, slashesDenoteHost);
  return u;
}

Url.prototype.parse = function(str, parseQueryString, hostDenotesSlash) {
  if (typeof str !== 'string') {
    throw new TypeError(`Parameter 'url' must be a string, not ` +
        typeof str);
  }
  // The field's value must always be an actual boolean.
  this._parsesQueryStrings = !!parseQueryString;
  var start = 0;
  var end = str.length - 1;

  // Trim leading and trailing ws.
  while (str.charCodeAt(start) <= 0x20 /*' '*/) start++;
  var trimmedStart = start;
  while (str.charCodeAt(end) <= 0x20 /*' '*/) end--;

  start = this._parseProtocol(str, start, end);

  // Javascript doesn't have host.
  if (this._protocol !== 'javascript') {
    start = this._parseHost(str, trimmedStart, start, end, hostDenotesSlash);
    var proto = this._protocol;
    if (!this._hostname &&
        (this.slashes || (proto && !_slashProtocols[proto])))
      this._hostname = this._host = '';
  }

  if (start <= end) {
    var ch = str.charCodeAt(start);
    if (ch === 0x2F /*'/'*/ || ch === 0x5C /*'\'*/)
      this._parsePath(str, start, end);
    else if (ch === 0x3F /*'?'*/)
      this._parseQuery(str, start, end);
    else if (ch === 0x23 /*'#'*/)
      this._parseHash(str, start, end);
    else if (this._protocol !== 'javascript')
      this._parsePath(str, start, end);
    else // For javascript the pathname is just the rest of it.
      this._pathname = str.slice(start, end + 1);
  }

  if (!this._pathname && this._hostname && _slashProtocols[this._protocol])
    this._pathname = '/';

  if (parseQueryString) {
    var search = this._search;
    if (search == null)
      search = this._search = '';
    if (search.charCodeAt(0) === 0x3F /*'?'*/)
      search = search.slice(1);
    // This calls a setter function, there is no .query data property.
    this.query = querystring.parse(search);
  }
};

function urlResolve(source, relative) {
  return urlParse(source, false, true).resolve(relative);
}

Url.prototype.resolve = function(relative) {
  return this.resolveObject(urlParse(relative, false, true)).format();
};

// Format a parsed object into a url string.
function urlFormat(obj) {
  // Ensure it's an object, and not a string url.
  // If it's an obj, this is a no-op.
  // this way, you can call url_format() on strings
  // to clean up potentially wonky urls.
  if (typeof obj === 'string') {
    obj = urlParse(obj);
  } else if (typeof obj !== 'object' || obj === null) {
    throw new TypeError('Parameter \'urlObj\' must be an object, not ' +
                        obj === null ? 'null' : typeof obj);
  } else if (!(obj instanceof Url)) {
    return Url.prototype.format.call(obj);
  }

  return obj.format();
}

Url.prototype.format = function() {
  var auth = this.auth || '';

  if (auth) {
    auth = encodeURIComponent(auth);
    auth = auth.replace(/%3A/i, ':');
    auth += '@';
  }

  var protocol = this.protocol || '';
  var pathname = this.pathname || '';
  var hash = this.hash || '';
  var search = this.search || '';
  var query = '';
  var hostname = this.hostname || '';
  var port = this.port || '';
  var host = false;
  var scheme = '';

  // Cache the result of the getter function.
  var q = this.query;
  if (q !== null && typeof q === 'object') {
    query = querystring.stringify(q);
  }

  if (!search) {
    search = query ? '?' + query : '';
  }

  if (protocol && protocol.charCodeAt(protocol.length - 1) !== 0x3A /*':'*/)
    protocol += ':';

  if (this.host) {
    host = auth + this.host;
  } else if (hostname) {
    host = auth + hostname + (port ? ':' + port : '');
  }

  var slashes = this.slashes ||
                ((!protocol || _slashProtocols[protocol]) && host !== false);

  if (protocol) scheme = protocol + (slashes ? '//' : '');
  else if (slashes) scheme = '//';

  if (slashes && pathname && pathname.charCodeAt(0) !== 0x2F /*'/'*/) {
    pathname = '/' + pathname;
  }
  if (search && search.charCodeAt(0) !== 0x3F /*'?'*/)
    search = '?' + search;
  if (hash && hash.charCodeAt(0) !== 0x23 /*'#'*/)
    hash = '#' + hash;

  pathname = escapePathName(pathname);
  search = escapeSearch(search);

  return scheme + (host === false ? '' : host) + pathname + search + hash;
};

function urlResolveObject(source, relative) {
  if (!source) return relative;
  return urlParse(source, false, true).resolveObject(relative);
}

Url.prototype.resolveObject = function(relative) {
  if (typeof relative === 'string')
    relative = urlParse(relative, false, true);

  var result = this._clone();

  // Hash is always overridden, no matter what.
  // even href='' will remove it.
  result._hash = relative._hash;

  // If the relative url is empty, then there's nothing left to do here.
  if (!relative.href) {
    result._href = '';
    return result;
  }

  // Hrefs like //foo/bar always cut to the protocol.
  if (relative.slashes && !relative._protocol) {
    relative._copyPropsTo(result, true);

    if (_slashProtocols[result._protocol] &&
        result._hostname && !result._pathname) {
      result._pathname = '/';
    }
    result._href = '';
    return result;
  }

  if (relative._protocol && relative._protocol !== result._protocol) {
    // If it's a known url protocol, then changing
    // the protocol does weird things
    // first, if it's not file:, then we MUST have a host,
    // and if there was a path
    // to begin with, then we MUST have a path.
    // if it is file:, then the host is dropped,
    // because that's known to be hostless.
    // anything else is assumed to be absolute.
    if (!_slashProtocols[relative._protocol]) {
      relative._copyPropsTo(result, false);
      result._href = '';
      return result;
    }

    result._protocol = relative._protocol;
    if (!relative._host &&
        !/^file:?$/.test(relative._protocol) &&
        relative._protocol !== 'javascript') {
      var relPath = (relative._pathname || '').split('/');
      while (relPath.length && !(relative._host = relPath.shift()));
      if (!relative._host) relative._host = '';
      if (!relative._hostname) relative._hostname = '';
      if (relPath[0] !== '') relPath.unshift('');
      if (relPath.length < 2) relPath.unshift('');
      result._pathname = relPath.join('/');
    } else {
      result._pathname = relative._pathname;
    }

    result._search = relative._search;
    result._host = relative._host || '';
    result._auth = relative._auth;
    result._hostname = relative._hostname || relative._host;
    result._port = relative._port;
    result.slashes = result.slashes || relative.slashes;
    result._href = '';
    return result;
  }

  var isSourceAbs =
      (result._pathname && result._pathname.charCodeAt(0) === 0x2F /*'/'*/);
  var isRelAbs = (
      relative._host ||
      (relative._pathname &&
      relative._pathname.charCodeAt(0) === 0x2F /*'/'*/));
  var mustEndAbs = (isRelAbs ||
                    isSourceAbs ||
                    (result._host && relative._pathname));

  var removeAllDots = mustEndAbs;

  var srcPath = result._pathname && result._pathname.split('/') || [];
  var relPath = relative._pathname && relative._pathname.split('/') || [];
  var psychotic = result._protocol && !_slashProtocols[result._protocol];

  // If the url is a non-slashed url, then relative
  // links like ../.. should be able
  // to crawl up to the hostname, as well.  This is strange.
  // result.protocol has already been set by now.
  // Later on, put the first path part into the host field.
  if (psychotic) {
    result._hostname = '';
    result._port = -1;
    if (result._host) {
      if (srcPath[0] === '') srcPath[0] = result._host;
      else srcPath.unshift(result._host);
    }
    result._host = '';
    if (relative._protocol) {
      relative._hostname = '';
      relative._port = -1;
      if (relative._host) {
        if (relPath[0] === '') relPath[0] = relative._host;
        else relPath.unshift(relative._host);
      }
      relative._host = '';
    }
    mustEndAbs = mustEndAbs && (relPath[0] === '' || srcPath[0] === '');
  }

  if (isRelAbs) {
    // it's absolute.
    result._host = relative._host ? relative._host : result._host;
    result._hostname =
        relative._hostname ? relative._hostname : result._hostname;
    result._search = relative._search;
    srcPath = relPath;
    // Fall through to the dot-handling below.
  } else if (relPath.length) {
    // It's relative
    // throw away the existing file, and take the new path instead.
    if (!srcPath) srcPath = [];
    srcPath.pop();
    srcPath = srcPath.concat(relPath);
    result._search = relative._search;
  } else if (relative._search) {
    // Just pull out the search.
    // like href='?foo'.
    // Put this after the other two cases because it simplifies the booleans
    if (psychotic) {
      result._hostname = result._host = srcPath.shift();
      // Occasionally the auth can get stuck only in host
      // this especialy happens in cases like
      // url.resolveObject('mailto:local1@domain1', 'local2@domain2').
      var authInHost = result._host && result._host.indexOf('@') > 0 ?
          result._host.split('@') : false;
      if (authInHost) {
        result._auth = authInHost.shift();
        result._host = result._hostname = authInHost.shift();
      }
    }
    result._search = relative._search;
    result._href = '';
    return result;
  }

  if (!srcPath.length) {
    // No path at all.  easy.
    // we've already handled the other stuff above.
    result._pathname = null;
    result._href = '';
    return result;
  }

  // If a url ENDs in . or .., then it must get a trailing slash.
  // however, if it ends in anything else non-slashy,
  // then it must NOT get a trailing slash.
  var last = srcPath.slice(-1)[0];
  var hasTrailingSlash = (
      (result._host || relative._host || srcPath.length > 1) &&
      (last === '.' || last === '..') || last === '');

  // Strip single dots, resolve double dots to parent dir
  // if the path tries to go above the root, `up` ends up > 0.
  var up = 0;
  for (var i = srcPath.length; i >= 0; i--) {
    last = srcPath[i];
    if (last === '.') {
      srcPath.splice(i, 1);
    } else if (last === '..') {
      srcPath.splice(i, 1);
      up++;
    } else if (up) {
      srcPath.splice(i, 1);
      up--;
    }
  }

  // If the path is allowed to go above the root, restore leading ..s.
  if (!mustEndAbs && !removeAllDots) {
    for (; up--; up) {
      srcPath.unshift('..');
    }
  }

  if (mustEndAbs && srcPath[0] !== '' &&
      (!srcPath[0] || srcPath[0].charCodeAt(0) !== 0x2F /*'/'*/)) {
    srcPath.unshift('');
  }

  if (hasTrailingSlash && (srcPath.join('/').substr(-1) !== '/')) {
    srcPath.push('');
  }

  var isAbsolute = srcPath[0] === '' ||
      (srcPath[0] && srcPath[0].charCodeAt(0) === 0x2F /*'/'*/);

  // put the host back
  if (psychotic) {
    result._hostname = result._host = isAbsolute ? '' :
        srcPath.length ? srcPath.shift() : '';
    // Occasionally the auth can get stuck only in host.
    // This especialy happens in cases like
    // url.resolveObject('mailto:local1@domain1', 'local2@domain2').
    var authInHost = result._host && result._host.indexOf('@') > 0 ?
        result._host.split('@') : false;
    if (authInHost) {
      result._auth = authInHost.shift();
      result._host = result._hostname = authInHost.shift();
    }
  }

  mustEndAbs = mustEndAbs || (result._host && srcPath.length);

  if (mustEndAbs && !isAbsolute) {
    srcPath.unshift('');
  }

  result._pathname = srcPath.length === 0 ? null : srcPath.join('/');
  result._auth = relative._auth || result._auth;
  result.slashes = result.slashes || relative.slashes;
  result._href = '';
  return result;
};


Url.prototype._parseProtocol = function(str, start, end) {
  var needsLowerCasing = false;
  var protocolCharacters = _protocolCharacters;

  for (var i = start; i <= end; ++i) {
    var ch = str.charCodeAt(i);

    if (ch === 0x3A /*':'*/) {
      if (i - start === 0)
        return start;
      var protocol = str.slice(start, i);
      if (needsLowerCasing) protocol = protocol.toLowerCase();
      this._protocol = protocol;
      return i + 1;
    } else if (protocolCharacters[ch] === 1) {
      if (ch < 0x61 /*'a'*/)
        needsLowerCasing = true;
    } else {
      return start;
    }

  }
  return start;
};

Url.prototype._parseAuth = function(str, start, end, decode) {
  var auth = str.slice(start, end + 1);
  if (decode) {
    auth = decodeURIComponent(auth);
  }
  this._auth = auth;
};

Url.prototype._parsePort = function(str, start, end) {
  // Distinguish between :0 and : (no port number at all).
  var hadChars = false;
  var hadValidPortTerminator = true;
  var checkLeadingZeroes = false;

  for (var i = start; i <= end; ++i) {
    var ch = str.charCodeAt(i);

    if (0x30 /*'0'*/ <= ch && ch <= 0x39 /*'9'*/) {
      if (i === start && ch === 0x30 /*'0'*/)
        checkLeadingZeroes = true;
      hadChars = true;
    } else {
      hadValidPortTerminator = false;
      if (_hostEndingCharacters[ch] === 1 ||
          _hostEndingCharactersNoPrependSlash[ch] === 1) {
        hadValidPortTerminator = true;
      } else {
        this._port = -2;
      }
      break;
    }

  }

  if (!hadChars || !hadValidPortTerminator)
    return 0;

  var portEnd = i;
  var port = str.slice(start, portEnd);

  if (checkLeadingZeroes) {
    var hadNonZero = false;
    for (var i = 0; i < port.length; ++i) {
      if (port.charCodeAt(i) !== 0x30 /*'0'*/) {
        hadNonZero = true;
        break;
      }
    }

    if (hadNonZero)
      port = -1;
    else
      port = '0';
  }
  this._port = port;
  return portEnd - start;
};

Url.prototype._hasValidPort = function() {
  return typeof this._port === 'string';
};

Url.prototype._parseHost = function(str,
                                    trimmedStart,
                                    start,
                                    end,
                                    slashesDenoteHost) {
  var hostEndingCharacters = _hostEndingCharacters;
  var first = str.charCodeAt(start);
  var second = str.charCodeAt(start + 1);
  if ((first === 0x2F /*'/'*/ || first === 0x5C /*'\'*/) &&
      (second === 0x2F /*'/'*/ || second === 0x5C /*'\'*/)) {
    this.slashes = true;

    // The string starts with // or \\.
    if (start === trimmedStart) {
      // The string is just '//' or '\\'.
      if (end - start === 1) return start;
      // If slashes do not denote host and there is no auth,
      // there is no host when the string starts with // or \\.
      var hasAuth =
          containsCharacter(str,
                            0x40 /*'@'*/,
                            trimmedStart + 2,
                            hostEndingCharacters);
      if (!hasAuth && !slashesDenoteHost) {
        this.slashes = null;
        return start;
      }
    }
    // There is a host that starts after the //.
    start += 2;
  }
  // If there is no slashes, there is no hostname if
  // 1. there was no protocol at all.
  else if (!this._protocol ||
      // 2. there was a protocol that requires slashes
      // e.g. in 'http:asd' 'asd' is not a hostname.
      _slashProtocols[this._protocol]
  ) {
    return start;
  }

  var needsLowerCasing = false;
  var idna = false;
  var hostNameStart = start;
  var hostNameEnd = end;
  var lastCh = -1;
  var portLength = 0;
  var charsAfterDot = 0;
  var authNeedsDecoding = false;

  var j = -1;

  // Find the last occurrence of an @-sign until hostending character is met
  // also mark if decoding is needed for the auth portion.
  for (var i = start; i <= end; ++i) {
    var ch = str.charCodeAt(i);
    if (ch === 0x40 /*'@'*/)
      j = i;
    else if (ch === 0x25 /*'%'*/)
      authNeedsDecoding = true;
    else if (hostEndingCharacters[ch] === 1)
      break;
  }

  // @-sign was found at index j, everything to the left from it
  // is auth part.
  if (j > -1) {
    this._parseAuth(str, start, j - 1, authNeedsDecoding);
    // Hostname starts after the last @-sign-
    start = hostNameStart = j + 1;
  }

  // Host name is starting with a [.
  if (str.charCodeAt(start) === 0x5B /*'['*/) {
    for (var i = start + 1; i <= end; ++i) {
      var ch = str.charCodeAt(i);

      // Assume valid IP6 is between the brackets.
      if (ch === 0x5D /*']'*/) {
        if (str.charCodeAt(i + 1) === 0x3A /*':'*/)
          portLength = this._parsePort(str, i + 2, end) + 1;

        var hostname = '[' + str.slice(start + 1, i).toLowerCase() + ']';
        this._hostname = hostname;
        this._host =
            this._hasValidPort() ? hostname + ':' + this._port : hostname;
        this._pathname = '/';
        return i + portLength + 1;
      }
    }
    // Empty hostname, [ starts a path.
    return start;
  }
  var hostEndingCharactersNoPrependSlash = _hostEndingCharactersNoPrependSlash;
  for (var i = start; i <= end; ++i) {
    if (charsAfterDot > 62) {
      this._hostname = this._host = str.slice(start, i);
      return i;
    }
    var ch = str.charCodeAt(i);

    if (ch === 0x3A /*':'*/) {
      portLength = this._parsePort(str, i + 1, end) + 1;
      hostNameEnd = i - 1;
      break;
    } else if (ch < 0x61 /*'a'*/) {
      if (ch === 0x2E /*'.'*/) {
        // TODO(petkaantonov) This error is originally ignored:
        // if (lastCh === 0x2E /*'.'*/ || lastCh === -1) {
        //   this.hostname = this.host = '';
        //   return start;
        // }
        charsAfterDot = -1;
      } else if (0x41 /*'A'*/ <= ch && ch <= 0x5A /*'Z'*/) {
        needsLowerCasing = true;
      }
      // Valid characters other than ASCII letters -, _, +, 0-9.
      else if (!(ch === 0x2D /*'-'*/ ||
              ch === 0x5F /*'_'*/ ||
              ch === 0x2B /*'+'*/ ||
              (0x30 /*'0'*/ <= ch && ch <= 0x39 /*'9'*/))) {
        if (hostEndingCharacters[ch] === 0 &&
            hostEndingCharactersNoPrependSlash[ch] === 0)
          this._prependSlash = true;
        hostNameEnd = i - 1;
        break;
      }
    } else if (ch >= 0x7B /*'{'*/) {
      if (ch <= 0x7E /*'~'*/) {
        if (hostEndingCharactersNoPrependSlash[ch] === 0) {
          this._prependSlash = true;
        }
        hostNameEnd = i - 1;
        break;
      }
      idna = true;
      needsLowerCasing = true;
    }
    lastCh = ch;
    charsAfterDot++;
  }

  // TODO(petkaantonov) This error is originally ignored:
  // if (lastCh === 0x2E /*'.'*/)
  //   hostNameEnd--

  if (hostNameEnd + 1 !== start &&
      hostNameEnd - hostNameStart <= 256) {
    var hostname = str.slice(hostNameStart, hostNameEnd + 1);
    if (needsLowerCasing)
      hostname = hostname.toLowerCase();
    if (idna)
      hostname = punycode.toASCII(hostname);

    this._hostname = hostname;
    this._host =
        this._hasValidPort() ? hostname + ':' + this._port : hostname;
  }

  return hostNameEnd + 1 + portLength;

};

Url.prototype._copyPropsTo = function(target, noProtocol) {
  if (!noProtocol) {
    target._protocol = this._protocol;
  }
  // Forces getter recalculation.
  target._href = null;
  target._host = this._host;
  target._hostname = this._hostname;
  target._pathname = this._pathname;
  target._search = this._search;
  target._parsesQueryStrings = this._parsesQueryStrings;
  target._prependSlash = this._prependSlash;
  target._port = this._port;
  target._auth = this._auth;
  target.slashes = this.slashes;
  target._query = this._query;
  target._hash = this._hash;
};

Url.prototype._clone = function() {
  var ret = new Url();
  ret._protocol = this._protocol;
  ret._href = this._href;
  ret._port = this._port;
  ret._prependSlash = this._prependSlash;
  ret._auth = this._auth;
  ret._hostname = this._hostname;
  ret._host = this._host;
  ret._hash = this._hash;
  ret._search = this._search;
  ret._pathname = this._pathname;
  ret._parsesQueryStrings = this._parsesQueryStrings;
  ret._query = this._query;
  ret.slashes = this.slashes;
  return ret;
};

Url.prototype.toJSON = function() {
  var ret = {
    href: this.href,
    pathname: this.pathname,
    path: this.path,
    protocol: this.protocol,
    query: this.query,
    port: this.port,
    auth: this.auth,
    hash: this.hash,
    host: this.host,
    hostname: this.hostname,
    search: this.search,
    slashes: this.slashes
  };
  var keys = Object.keys(this);
  for (var i = 0; i < keys.length; ++i) {
    var key = keys[i];
    if (noSerializePattern.test(key)) continue;
    ret[key] = this[key];

  }
  return ret;
};

Url.prototype._parsePath = function(str, start, end) {
  var pathStart = start;
  var pathEnd = end;
  var escape = false;
  var autoEscapeCharacters = _autoEscapeCharacters;
  var prePath = this._port === -2 ? '/:' : '';

  for (var i = start; i <= end; ++i) {
    var ch = str.charCodeAt(i);

    if (ch === 0x23 /*'#'*/) {
      this._parseHash(str, i, end);
      pathEnd = i - 1;
      break;
    } else if (ch === 0x3F /*'?'*/) {
      this._parseQuery(str, i, end);
      pathEnd = i - 1;
      break;
    } else if (!escape && autoEscapeCharacters[ch] === 1) {
      escape = true;
    }
  }

  if (pathStart > pathEnd) {
    this._pathname = prePath === '' ? '/' : prePath;
    return;
  }

  var path;
  if (escape) {
    path = getComponentEscaped(str, pathStart, pathEnd, false);
  } else {
    path = str.slice(pathStart, pathEnd + 1);
  }

  this._pathname = prePath === '' ?
      (this._prependSlash ? '/' + path : path) : prePath + path;
};

Url.prototype._parseQuery = function(str, start, end) {
  var queryStart = start;
  var queryEnd = end;
  var escape = false;
  var autoEscapeCharacters = _autoEscapeCharacters;

  for (var i = start; i <= end; ++i) {
    var ch = str.charCodeAt(i);

    if (ch === 0x23 /*'#'*/) {
      this._parseHash(str, i, end);
      queryEnd = i - 1;
      break;
    } else if (!escape && autoEscapeCharacters[ch] === 1)
      escape = true;
  }

  if (queryStart > queryEnd) {
    this._search = '';
    return;
  }

  var query = escape ?
      getComponentEscaped(str, queryStart, queryEnd, true) :
      str.slice(queryStart, queryEnd + 1);

  this._search = query;
};

Url.prototype._parseHash = function(str, start, end) {
  if (start > end) {
    this._hash = '';
    return;
  }
  this._hash = getComponentEscaped(str, start, end, true);
};

Object.defineProperty(Url.prototype, 'port', {
  get: function() {
    if (this._hasValidPort())
      return this._port;

    return null;
  },
  set: function(v) {
    if (v === null) {
      this._port = -1;
      if (this._host)
        this._host = null;
    } else {
      this._port = toPortNumber(v);

      if (this._hostname)
        this._host = this._hostname + ':' + this._port;
      else
        this._host = ':' + this._port;
    }
    this._href = '';
  },
  enumerable: true,
  configurable: true
});

Object.defineProperty(Url.prototype, 'query', {
  get: function() {
    var query = this._query;
    if (query != null)
      return query;

    var search = this._search;

    if (search) {
      if (search.charCodeAt(0) === 0x3F /*'?'*/)
        search = search.slice(1);
      if (search !== '') {
        this._query = search;
        return search;
      }
    }
    return search;
  },
  set: function(v) {
    if (typeof v === 'string') {
      if (v !== '') this.search = '?' + v;
      this._query = v;
    } else if (v !== null && typeof v === 'object') {
      var string = querystring.stringify(v);
      this._search = string !== '' ? '?' + querystring.stringify(v) : '';
      this._query = v;
    } else {
      this._query = this._search = null;
    }
    this._href = '';
  },
  enumerable: true,
  configurable: true
});

Object.defineProperty(Url.prototype, 'path', {
  get: function() {
    var p = this.pathname || '';
    var s = this.search || '';
    if (p || s)
      return p + s;
    return (p == null && s) ? ('/' + s) : null;
  },
  set: function(v) {
    if (v === null) {
      this._pathname = this._search = null;
      return;
    }
    var path = '' + v;
    var matches = path.match(/([^\?]*)(\?.*)$/);

    if (matches) {
      this.pathname = matches[1];
      this.search = matches[2];
    } else {
      this.pathname = path;
      this.search = null;
    }
    this._href = '';
  },
  enumerable: true,
  configurable: true
});

Object.defineProperty(Url.prototype, 'protocol', {
  get: function() {
    var proto = this._protocol;

    if (typeof proto === 'string' && proto.length > 0) {
      if (proto.charCodeAt(proto.length - 1) !== 0x3A/*':'*/)
        return proto + ':';
      return proto;
    }
    return proto;
  },
  set: function(v) {
    if (v === null) {
      this._protocol = null;
    } else {
      var proto = '' + v;
      if (proto.length > 0) {
        if (proto.charCodeAt(proto.length - 1) !== 0x3A /*':'*/)
          proto = proto + ':';
        this._parseProtocol(proto, 0, proto.length - 1);
        this._href = '';
      }
    }
  },
  enumerable: true,
  configurable: true
});

Object.defineProperty(Url.prototype, 'href', {
  get: function() {
    var href = this._href;
    if (!href)
      href = this._href = this.format();
    return href;
  },
  set: function(v) {
    this._href = '';
    var parsesQueryStrings = this._parsesQueryStrings;
    // Reset properties.
    Url.call(this);
    if (v !== null && v !== '')
      this.parse('' + v, parsesQueryStrings, false);
  },
  enumerable: true,
  configurable: true
});

Object.defineProperty(Url.prototype, 'auth', {
  get: function() {
    return this._auth;
  },
  set: function(v) {
    this._auth = v === null ? null : '' + v;
    this._href = '';
  },
  enumerable: true,
  configurable: true
});

// host = hostname + port.
Object.defineProperty(Url.prototype, 'host', {
  get: function() {
    return this._host;
  },
  set: function(v) {
    if (v === null) {
      this._port = -1;
      this._hostname = this._host = null;
    } else {
      var host = '' + v;
      var matches = host.match(/(.*):(.+)$/);

      if (matches) {
        this._hostname = encodeURIComponent(matches[1]);
        this._port = toPortNumber(matches[2]);
        this._host = this._hostname + ':' + this._port;
      } else {
        this._port = -1;
        this._hostname = this._host = encodeURIComponent(host);
      }
      this._href = '';
    }
  },
  enumerable: true,
  configurable: true
});

Object.defineProperty(Url.prototype, 'hostname', {
  get: function() {
    return this._hostname;
  },
  set: function(v) {
    if (v === null) {
      this._hostname = null;

      if (this._hasValidPort())
        this._host = ':' + this._port;
      else
        this._host = null;
    } else {
      var hostname = encodeURIComponent('' + v);
      this._hostname = hostname;

      if (this._hasValidPort())
        this._host = hostname + ':' + this._port;
      else
        this._host = hostname;

      this._href = '';
    }
  },
  enumerable: true,
  configurable: true
});

Object.defineProperty(Url.prototype, 'hash', {
  get: function() {
    return this._hash;
  },
  set: function(v) {
    if (v === null) {
      this._hash = null;
    } else {
      var hash = '' + v;
      if (hash.charCodeAt(0) !== 0x23 /*'#'*/) {
        hash = '#' + hash;
      }
      this._hash = hash;
      this._href = '';
    }
  },
  enumerable: true,
  configurable: true
});

Object.defineProperty(Url.prototype, 'search', {
  get: function() {
    return this._search;
  },
  set: function(v) {
    if (v === null) {
      this._search = this._query = null;
    } else {
      var search = escapeSearch('' + v);

      if (search.charCodeAt(0) !== 0x3F /*'?'*/)
        search = '?' + search;

      this._search = search;

      if (this._parsesQueryStrings) {
        this.query = querystring.parse(search.slice(1));
      }
      this._href = '';
    }
  },
  enumerable: true,
  configurable: true
});

Object.defineProperty(Url.prototype, 'pathname', {
  get: function() {
    return this._pathname;
  },
  set: function(v) {
    if (v === null) {
      this._pathname = null;
    } else {
      var pathname = escapePathName('' + v).replace(/\\/g, '/');

      if (this.hostname &&
          _slashProtocols[this._protocol] &&
          pathname.charCodeAt(0) !== 0x2F /*'/'*/) {
        pathname = '/' + pathname;
      }

      this._pathname = pathname;
      this._href = '';
    }
  },
  enumerable: true,
  configurable: true
});

// Search `char1` (integer code for a character) in `string`
// starting from `fromIndex` and ending at `string.length - 1`
// or when a stop character is found.
function containsCharacter(string, char1, fromIndex, stopCharacterTable) {
  var len = string.length;
  for (var i = fromIndex; i < len; ++i) {
    var ch = string.charCodeAt(i);

    if (ch === char1)
      return true;
    else if (stopCharacterTable[ch] === 1)
      return false;
  }
  return false;
}

// See if `char1` or `char2` (integer codes for characters)
// is contained in `string`.
function containsCharacter2(string, char1, char2) {
  for (var i = 0, len = string.length; i < len; ++i) {
    var ch = string.charCodeAt(i);
    if (ch === char1 || ch === char2)
      return true;
  }
  return false;
}

// Makes an array of 128 uint8's which represent boolean values.
// Spec is an array of ascii code points or ascii code point ranges
// ranges are expressed as [start, end].

// For example, to create a table with the characters
// 0x30-0x39 (decimals '0' - '9') and
// 0x7A (lowercaseletter 'z') as `true`:

// var a = makeAsciiTable([[0x30, 0x39], 0x7A]);
// a[0x30]; //1
// a[0x15]; //0
// a[0x35]; //1
function makeAsciiTable(spec) {
  var ret = new Uint8Array(128);
  spec.forEach(function(item) {
    if (typeof item === 'number') {
      ret[item] = 1;
    } else {
      var start = item[0];
      var end = item[1];
      for (var j = start; j <= end; ++j) {
        ret[j] = 1;
      }
    }
  });

  return ret;
}

function escapePathName(pathname) {
  if (!containsCharacter2(pathname, 0x23 /*'#'*/, 0x3F /*'?'*/))
    return pathname;

  return pathname.replace(/[?#]/g, function(match) {
    return encodeURIComponent(match);
  });
}

function escapeSearch(search) {
  if (!containsCharacter2(search, 0x23 /*'#'*/, -1))
    return search;

  return search.replace(/#/g, function(match) {
    return encodeURIComponent(match);
  });
}

function getComponentEscaped(str, start, end, isAfterQuery) {
  var cur = start;
  var i = start;
  var ret = '';
  var autoEscapeMap = isAfterQuery ? _afterQueryAutoEscapeMap : _autoEscapeMap;
  for (; i <= end; ++i) {
    var ch = str.charCodeAt(i);
    var escaped = autoEscapeMap[ch];

    if (escaped !== '' && escaped !== undefined) {
      if (cur < i) ret += str.slice(cur, i);
      ret += escaped;
      cur = i + 1;
    }
  }
  if (cur < i + 1) ret += str.slice(cur, i);
  return ret;
}

function toPortNumber(v) {
  v = parseInt(v);
  if (!Number.isFinite(v))
    v = 0;

  v = Math.max(0, v) % 0x10000;
  return '' + v;
}

// Optimize back from normalized object caused by non-identifier keys.
function FakeConstructor() {}
FakeConstructor.prototype = _slashProtocols;
/*jshint nonew: false */
new FakeConstructor();
