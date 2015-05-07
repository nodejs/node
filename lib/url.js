'use strict';

const punycode = require('punycode');

exports.parse = urlParse;
exports.resolve = urlResolve;
exports.resolveObject = urlResolveObject;
exports.format = urlFormat;

exports.Url = Url;

function Url() {
  this.protocol = null;
  this.slashes = null;
  this.auth = null;
  this.host = null;
  this.port = null;
  this.hostname = null;
  this.hash = null;
  this.search = null;
  this.query = null;
  this.pathname = null;
  this.path = null;
  this.href = null;
  this._prependSlash = false;
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

function urlParse(url, parseQueryString, slashesDenoteHost) {
  if (url instanceof Url)
    return url;

  var u = new Url();
  u.parse(url, parseQueryString, slashesDenoteHost);
  u.href = u.format();
  return u;
}

Url.prototype.parse = function(str, parseQueryString, slashesDenoteHost) {
  if (typeof str !== 'string') {
    throw new TypeError(`Parameter 'url' must be a string, not ` +
        typeof str);
  }
  var start = 0;
  var end = str.length - 1;

  // Trim leading and trailing ws.
  while (str.charCodeAt(start) <= 0x20 /*' '*/) start++;
  var trimmedStart = start;
  while (str.charCodeAt(end) <= 0x20 /*' '*/) end--;

  start = this._parseProtocol(str, start, end);

  // Javascript doesn't have host.
  if (this.protocol !== 'javascript:') {
    start = this._parseHost(str, trimmedStart, start, end, slashesDenoteHost);
    var proto = this.protocol;
    if (this._isEmpty(this.hostname) &&
        (this.slashes || (!this._isEmpty(proto) && !_slashProtocols[proto])))
      this.hostname = this.host = '';
  }

  if (start <= end) {
    var ch = str.charCodeAt(start);
    if (ch === 0x2F /*'/'*/ || ch === 0x5C /*'\'*/)
      this._parsePath(str, start, end);
    else if (ch === 0x3F /*'?'*/)
      this._parseQuery(str, start, end);
    else if (ch === 0x23 /*'#'*/)
      this._parseHash(str, start, end);
    else if (this.protocol !== 'javascript:')
      this._parsePath(str, start, end);
    else // For javascript the pathname is just the rest of it.
      this.pathname = str.slice(start, end + 1);
  }

  if (this._isEmpty(this.pathname) &&
      !this._isEmpty(this.hostname) &&
      _slashProtocols[this.protocol])
    this.pathname = '/';


  var pathname = this.pathname;
  var search = this.search;

  if (this._isEmpty(search)) {
    // Bug-to-bug compat.
    if (parseQueryString)
      this.search = '';

    this.query = parseQueryString ? {} : search;

    if (!this._isEmpty(pathname))
      this.path = pathname;
  } else {
    var query = search.slice(1);
    this.query = parseQueryString ? querystring.parse(query) : query;
    this.path = this._isEmpty(pathname) ? search : pathname + search;
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

  var q = this.query;
  if (q !== null && typeof q === 'object')
    query = querystring.stringify(q);

  if (!search)
    search = query ? '?' + query : '';

  if (protocol && protocol.charCodeAt(protocol.length - 1) !== 0x3A /*':'*/)
    protocol += ':';

  if (this.host) {
    host = auth + this.host;
  } else if (hostname) {
    if (containsCharacter2(hostname, 0x3A/*':'*/, -1))
      hostname = '[' + hostname + ']';
    host = auth + hostname + (port ? ':' + port : '');
  }

  var slashes = this.slashes ||
                ((!protocol || _slashProtocols[protocol]) && host !== false);

  if (protocol)
    scheme = protocol + (slashes ? '//' : '');
  else if (slashes)
    scheme = '//';

  if (slashes && pathname && pathname.charCodeAt(0) !== 0x2F /*'/'*/)
    pathname = '/' + pathname;
  if (search && search.charCodeAt(0) !== 0x3F /*'?'*/)
    search = '?' + search;
  if (hash && hash.charCodeAt(0) !== 0x23 /*'#'*/)
    hash = '#' + hash;

  pathname = escapePathName(pathname);
  search = escapeSearch(search);

  return scheme + (host === false ? '' : host) + pathname + search + hash;
};

Url.prototype._clone = function() {
  var ret = new Url();
  ret.protocol = this.protocol;
  ret.slashes = this.slashes;
  ret.auth = this.auth;
  ret.host = this.host;
  ret.port = this.port;
  ret.hostname = this.hostname;
  ret.hash = this.hash;
  ret.search = this.search;
  ret.query = this.query;
  ret.pathname = this.pathname;
  ret.path = this.path;
  ret.href = this.href;
  ret._prependSlash = this._prependSlash;
  return ret;
};

Url.prototype._copyPropsTo = function(target, noProtocol) {
  if (!noProtocol)
    target.protocol = this.protocol;

  target.slashes = this.slashes;
  target.auth = this.auth;
  target.host = this.host;
  target.port = this.port;
  target.hostname = this.hostname;
  target.hash = this.hash;
  target.search = this.search;
  target.query = this.query;
  target.pathname = this.pathname;
  target.path = this.path;
  target.href = this.href;
  target._prependSlash = this._prependSlash;
};

function urlResolveObject(source, relative) {
  if (!source)
    return relative;
  return urlParse(source, false, true).resolveObject(relative);
}

Url.prototype._resolveEmpty = function() {
  var ret = this._clone();
  ret.hash = null;
  ret.href = ret.format();
  return ret;
};

Url.prototype.resolveObject = function(relative) {
  if (typeof relative === 'string') {
    if (relative.length === 0)
      return this._resolveEmpty();

    var u = new Url();
    u.parse(relative, false, true);
    relative = u;
  } else if (this._isEmpty(relative.href)) {
    return this._resolveEmpty();
  }

  var result = this._clone();

  // Hash is always overridden, no matter what.
  // even href='' will remove it.
  result.hash = relative.hash;

  // Hrefs like //foo/bar always cut to the protocol.
  if (relative.slashes && this._isEmpty(relative.protocol)) {
    relative._copyPropsTo(result, true);

    if (_slashProtocols[result.protocol] &&
        !this._isEmpty(result.hostname) &&
        this._isEmpty(result.pathname))
      result.path = result.pathname = '/';

    result.href = result.format();
    return result;
  }

  if (!this._isEmpty(relative.protocol) &&
      relative.protocol !== result.protocol) {
    // If it's a known url protocol, then changing
    // the protocol does weird things
    // first, if it's not file:, then we MUST have a host,
    // and if there was a path
    // to begin with, then we MUST have a path.
    // if it is file:, then the host is dropped,
    // because that's known to be hostless.
    // anything else is assumed to be absolute.
    if (!_slashProtocols[relative.protocol]) {
      relative._copyPropsTo(result, false);
      result.href = result.format();
      return result;
    }

    result.protocol = relative.protocol;
    if (this._isEmpty(relative.host) &&
        !/^file:?$/.test(relative.protocol) &&
        relative.protocol !== 'javascript:') {
      var relPath = (relative.pathname || '').split('/');
      while (relPath.length && !(relative.host = relPath.shift()));
      if (this._isEmpty(relative.host))
        relative.host = '';
      if (this._isEmpty(relative.hostname))
        relative.hostname = '';
      if (relPath[0] !== '')
        relPath.unshift('');
      if (relPath.length < 2)
        relPath.unshift('');
      result.pathname = relPath.join('/');
    } else {
      result.pathname = relative.pathname;
    }

    result.search = relative.search;
    result.query = relative.query;
    result.host = relative.host || '';
    result.auth = relative.auth;
    result.hostname = relative.hostname || relative.host;
    result.port = relative.port;
    // to support http.request
    if (result.pathname || result.search) {
      var p = result.pathname || '';
      var s = result.search || '';
      result.path = p + s;
    }
    result.slashes = result.slashes || relative.slashes;
    result.href = result.format();
    return result;
  }

  var isSourceAbs =
      (!this._isEmpty(result.pathname) &&
      result.pathname.charCodeAt(0) === 0x2F /*'/'*/);
  var isRelAbs = (
      !this._isEmpty(relative.host) ||
      (!this._isEmpty(relative.pathname) &&
      relative.pathname.charCodeAt(0) === 0x2F /*'/'*/));

  var mustEndAbs = (isRelAbs ||
                    isSourceAbs ||
                    (!this._isEmpty(result.host) &&
                    !this._isEmpty(relative.pathname)));

  var removeAllDots = mustEndAbs;

  var srcPath = !this._isEmpty(result.pathname) &&
      result.pathname.split('/') || [];
  var relPath = !this._isEmpty(relative.pathname) &&
      relative.pathname.split('/') || [];
  var psychotic = !this._isEmpty(result.protocol) &&
      !_slashProtocols[result.protocol];

  // If the url is a non-slashed url, then relative
  // links like ../.. should be able
  // to crawl up to the hostname, as well.  This is strange.
  // result.protocol has already been set by now.
  // Later on, put the first path part into the host field.
  if (psychotic) {
    result.hostname = '';
    result.port = null;
    if (!this._isEmpty(result.host)) {
      if (srcPath[0] === '')
        srcPath[0] = result.host;
      else
        srcPath.unshift(result.host);
    }
    result.host = '';
    if (!this._isEmpty(relative.protocol)) {
      relative.hostname = null;
      relative.port = null;
      if (relative.host) {
        if (relPath[0] === '')
          relPath[0] = relative.host;
        else
          relPath.unshift(relative.host);
      }
      relative.host = '';
    }
    mustEndAbs = mustEndAbs && (relPath[0] === '' || srcPath[0] === '');
  }

  if (isRelAbs) {
    // it's absolute.
    result.host = !this._isEmpty(relative.host) ? relative.host : result.host;
    result.hostname =
        !this._isEmpty(relative.hostname) ? relative.hostname : result.hostname;
    result.search = relative.search;
    result.query = relative.query;
    srcPath = relPath;
    // Fall through to the dot-handling below.
  } else if (relPath.length) {
    // It's relative
    // throw away the existing file, and take the new path instead.
    if (!srcPath)
      srcPath = [];
    srcPath.pop();
    srcPath = srcPath.concat(relPath);
    result.search = relative.search;
    result.query = relative.query;
  } else if (!this._isEmpty(relative.search)) {
    // Just pull out the search.
    // like href='?foo'.
    // Put this after the other two cases because it simplifies the booleans
    if (psychotic) {
      result.hostname = result.host = srcPath.shift();
      // Occasionally the auth can get stuck only in host
      // this especialy happens in cases like
      // url.resolveObject('mailto:local1@domain1', 'local2@domain2').
      var authInHost = !this._isEmpty(result.host) &&
          result.host.indexOf('@') > 0 ? result.host.split('@') : false;
      if (authInHost !== false) {
        result.auth = authInHost.shift();
        result.host = result.hostname = authInHost.shift();
      }
    }
    result.search = relative.search;
    result.query = relative.query;
    result.path = result.pathname !== null ?
        result.pathname + result.search : result.search;
    result.href = result.format();
    return result;
  }

  if (!srcPath.length) {
    // No path at all.  easy.
    // we've already handled the other stuff above.
    result.pathname = null;
    result.path = this._isEmpty(result.search) ? null : '/' + result.search;
    result.href = result.format();
    return result;
  }

  // If a url ENDs in . or .., then it must get a trailing slash.
  // however, if it ends in anything else non-slashy,
  // then it must NOT get a trailing slash.
  var last = srcPath.slice(-1)[0];
  var hasTrailingSlash = (
      (!this._isEmpty(result.host) ||
      !this._isEmpty(relative.host) ||
      srcPath.length > 1) &&
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
    for (; up--; up)
      srcPath.unshift('..');
  }

  if (mustEndAbs && srcPath[0] !== '' &&
      (!srcPath[0] || srcPath[0].charCodeAt(0) !== 0x2F /*'/'*/)) {
    srcPath.unshift('');
  }

  if (hasTrailingSlash && (srcPath.join('/').substr(-1) !== '/'))
    srcPath.push('');

  var isAbsolute = srcPath[0] === '' ||
      (srcPath[0] && srcPath[0].charCodeAt(0) === 0x2F /*'/'*/);

  // put the host back
  if (psychotic) {
    result.hostname = result.host = isAbsolute ? '' :
        srcPath.length ? srcPath.shift() : '';
    // Occasionally the auth can get stuck only in host
    // this especialy happens in cases like
    // url.resolveObject('mailto:local1@domain1', 'local2@domain2').
    var authInHost = !this._isEmpty(result.host) &&
        result.host.indexOf('@') > 0 ? result.host.split('@') : false;
    if (authInHost !== false) {
      result.auth = authInHost.shift();
      result.host = result.hostname = authInHost.shift();
    }
  }

  mustEndAbs = mustEndAbs || (result.host && srcPath.length);

  if (mustEndAbs && !isAbsolute)
    srcPath.unshift('');

  if (srcPath.length === 0) {
    result.pathname = null;
    result.path = this._isEmpty(result.search) ? null : result.search;
  } else {
    var pathname = srcPath.join('/');
    result.pathname = pathname;
    result.path =
        this._isEmpty(result.search) ? pathname : pathname + result.search;
  }

  result.auth = relative.auth || result.auth;
  result.slashes = result.slashes || relative.slashes;
  result.href = result.format();
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
      var protocol = str.slice(start, i + 1);
      if (needsLowerCasing)
        protocol = protocol.toLowerCase();
      this.protocol = protocol;
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
  if (decode)
    auth = decodeURIComponent(auth);
  this.auth = auth;
};

Url.prototype._parsePort = function(str, start, end) {
  for (var i = start; i <= end; ++i) {
    var ch = str.charCodeAt(i);

    if (!(0x30 /*'0'*/ <= ch && ch <= 0x39 /*'9'*/)) {
      this._prependSlash = true;
      return false;
    }
  }

  if (i > start)
    this.port = str.slice(start, i);
  return true;
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
      if (end - start === 1)
        return start;
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
  else if (this.protocol === null ||
      // 2. there was a protocol that requires slashes
      // e.g. in 'http:asd' 'asd' is not a hostname.
      _slashProtocols[this.protocol]
  ) {
    return start;
  }

  var needsLowerCasing = false;
  var idna = false;
  var hostNameStart = start;
  var hostnameEnd = end;
  var lastCh = -1;
  var hostEndsAtPortEnd = true;
  var portStart = -1;
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

  var hostEndingCharactersNoPrependSlash = _hostEndingCharactersNoPrependSlash;
  // Host name is starting with a [.
  if (str.charCodeAt(start) === 0x5B /*'['*/) {
    for (var i = start + 1; i <= end; ++i) {
      var ch = str.charCodeAt(i);

      // Assume valid IP6 is between the brackets.
      if (ch === 0x5D /*']'*/) {
        var ip6End = i;
        for (i = i + 1; i <= end; ++i) {
          var ch = str.charCodeAt(i);
          if (ch === 0x3A /*':'*/)
            portStart = i + 1;
          else if (hostEndingCharacters[ch] === 1 ||
                   hostEndingCharactersNoPrependSlash[ch] === 1)
            break;
        }

        var portEnd = i - 1;
        hostnameEnd = portStart !== -1 ? portStart - 1 : ip6End + 1;

        var isInvalidHost = portStart !== -1 && ip6End + 1 !== portStart - 1;
        var hostname =
            isInvalidHost ? str.slice(start, hostnameEnd).toLowerCase() :
                str.slice(start + 1, hostnameEnd - 1).toLowerCase();

        if (portStart !== -1)
          hostEndsAtPortEnd = this._parsePort(str, portStart, portEnd);

        this.hostname = hostname;

        if (!isInvalidHost)
          hostname = '[' + hostname + ']';

        this.host =
            this._isEmpty(this.port) ? hostname : hostname + ':' + this.port;
        this.pathname = '/';
        return (hostEndsAtPortEnd ? portEnd + 1 : portStart - 1);
      }
    }
    // Empty hostname, [ starts a path.
    return start;
  }

  for (var i = start; i <= end; ++i) {
    if (charsAfterDot > 62) {
      this.hostname = this.host = str.slice(start, i);
      return i;
    }
    var ch = str.charCodeAt(i);

    if (ch === 0x3A /*':'*/) {
      portStart = i + 1;
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
        hostnameEnd = i - 1;
        break;
      }
    } else if (ch >= 0x7B /*'{'*/) {
      if (ch <= 0x7E /*'~'*/) {
        if (hostEndingCharactersNoPrependSlash[ch] === 0)
          this._prependSlash = true;
        hostnameEnd = i - 1;
        break;
      }
      idna = true;
      needsLowerCasing = true;
    }
    lastCh = ch;
    charsAfterDot++;
  }

  var portEnd = i - 1;
  hostnameEnd = portStart !== -1 ? portStart - 2 : hostnameEnd;

  if (portStart !== -1)
    hostEndsAtPortEnd = this._parsePort(str, portStart, portEnd);

  // TODO(petkaantonov) This error is originally ignored:
  // if (lastCh === 0x2E /*'.'*/)
  //   hostnameEnd--

  if (hostnameEnd + 1 !== start &&
      hostnameEnd - hostNameStart <= 256) {
    var hostname = str.slice(hostNameStart, hostnameEnd + 1);
    if (needsLowerCasing)
      hostname = hostname.toLowerCase();
    if (idna)
      hostname = punycode.toASCII(hostname);

    this.hostname = hostname;
    this.host =
        this._isEmpty(this.port) ? hostname : hostname + ':' + this.port;
  }

  return (hostEndsAtPortEnd ? portEnd + 1 : portStart - 1);
};

Url.prototype._parsePath = function(str, start, end) {
  var pathStart = start;
  var pathEnd = end;
  var escape = false;
  var autoEscapeCharacters = _autoEscapeCharacters;

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
    this.pathname = '/';
    return;
  }

  var path;
  if (escape)
    path = getComponentEscaped(str, pathStart, pathEnd, false);
  else
    path = str.slice(pathStart, pathEnd + 1);

  this.pathname = (this._prependSlash ? '/' + path : path);
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
    } else if (!escape && autoEscapeCharacters[ch] === 1) {
      escape = true;
    }
  }

  if (queryStart > queryEnd) {
    this.search = '';
    return;
  }

  var query = escape ?
      getComponentEscaped(str, queryStart, queryEnd, true) :
      str.slice(queryStart, queryEnd + 1);

  this.search = query;
};

Url.prototype._parseHash = function(str, start, end) {
  if (start > end) {
    this.hash = '';
    return;
  }
  this.hash = getComponentEscaped(str, start, end, true);
};

// This must be a method because this getting inlined is crucial.
Url.prototype._isEmpty = function(value) {
  return value === null || value === '' || value === undefined;
};


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
      if (cur < i)
        ret += str.slice(cur, i);
      ret += escaped;
      cur = i + 1;
    }
  }
  if (cur < i + 1)
    ret += str.slice(cur, i);
  return ret;
}

// Optimize back from normalized object caused by non-identifier keys.
function FakeConstructor() {}
FakeConstructor.prototype = _slashProtocols;
/*jshint nonew: false */
new FakeConstructor();
