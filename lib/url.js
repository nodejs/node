// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';

const { toASCII } = process.binding('config').hasIntl ?
  process.binding('icu') : require('punycode');

const { hexTable } = require('internal/querystring');

// WHATWG URL implementation provided by internal/url
const {
  URL,
  URLSearchParams,
  domainToASCII,
  domainToUnicode,
  formatSymbol
} = require('internal/url');

// Original url.parse() API

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
}

// Reference: RFC 3986, RFC 1808, RFC 2396

// define these here so at least they only have to be
// compiled once on the first module load.
const protocolPattern = /^[a-z0-9.+-]+:/i;
const portPattern = /:[0-9]*$/;
const hostPattern = /^\/\/[^@/]+@[^@/]+/;

// Special case for a simple path URL
const simplePathPattern = /^(\/\/?(?!\/)[^?\s]*)(\?[^\s]*)?$/;

const hostnameMaxLen = 255;
// protocols that can allow "unsafe" and "unwise" chars.
const unsafeProtocol = {
  'javascript': true,
  'javascript:': true
};
// protocols that never have a hostname.
const hostlessProtocol = {
  'javascript': true,
  'javascript:': true
};
// protocols that always contain a // bit.
const slashedProtocol = {
  'http': true,
  'http:': true,
  'https': true,
  'https:': true,
  'ftp': true,
  'ftp:': true,
  'gopher': true,
  'gopher:': true,
  'file': true,
  'file:': true
};
const querystring = require('querystring');

function urlParse(url, parseQueryString, slashesDenoteHost) {
  if (url instanceof Url) return url;

  var u = new Url();
  u.parse(url, parseQueryString, slashesDenoteHost);
  return u;
}

Url.prototype.parse = function parse(url, parseQueryString, slashesDenoteHost) {
  if (typeof url !== 'string') {
    throw new TypeError('Parameter "url" must be a string, not ' + typeof url);
  }

  // Copy chrome, IE, opera backslash-handling behavior.
  // Back slashes before the query string get converted to forward slashes
  // See: https://code.google.com/p/chromium/issues/detail?id=25916
  var hasHash = false;
  var start = -1;
  var end = -1;
  var rest = '';
  var lastPos = 0;
  var i = 0;
  for (var inWs = false, split = false; i < url.length; ++i) {
    const code = url.charCodeAt(i);

    // Find first and last non-whitespace characters for trimming
    const isWs = code === 32/* */ ||
                 code === 9/*\t*/ ||
                 code === 13/*\r*/ ||
                 code === 10/*\n*/ ||
                 code === 12/*\f*/ ||
                 code === 160/*\u00A0*/ ||
                 code === 65279/*\uFEFF*/;
    if (start === -1) {
      if (isWs)
        continue;
      lastPos = start = i;
    } else {
      if (inWs) {
        if (!isWs) {
          end = -1;
          inWs = false;
        }
      } else if (isWs) {
        end = i;
        inWs = true;
      }
    }

    // Only convert backslashes while we haven't seen a split character
    if (!split) {
      switch (code) {
        case 35: // '#'
          hasHash = true;
        // Fall through
        case 63: // '?'
          split = true;
          break;
        case 92: // '\\'
          if (i - lastPos > 0)
            rest += url.slice(lastPos, i);
          rest += '/';
          lastPos = i + 1;
          break;
      }
    } else if (!hasHash && code === 35/*#*/) {
      hasHash = true;
    }
  }

  // Check if string was non-empty (including strings with only whitespace)
  if (start !== -1) {
    if (lastPos === start) {
      // We didn't convert any backslashes

      if (end === -1) {
        if (start === 0)
          rest = url;
        else
          rest = url.slice(start);
      } else {
        rest = url.slice(start, end);
      }
    } else if (end === -1 && lastPos < url.length) {
      // We converted some backslashes and have only part of the entire string
      rest += url.slice(lastPos);
    } else if (end !== -1 && lastPos < end) {
      // We converted some backslashes and have only part of the entire string
      rest += url.slice(lastPos, end);
    }
  }

  if (!slashesDenoteHost && !hasHash) {
    // Try fast path regexp
    const simplePath = simplePathPattern.exec(rest);
    if (simplePath) {
      this.path = rest;
      this.href = rest;
      this.pathname = simplePath[1];
      if (simplePath[2]) {
        this.search = simplePath[2];
        if (parseQueryString) {
          this.query = querystring.parse(this.search.slice(1));
        } else {
          this.query = this.search.slice(1);
        }
      } else if (parseQueryString) {
        this.search = '';
        this.query = Object.create(null);
      }
      return this;
    }
  }

  var proto = protocolPattern.exec(rest);
  if (proto) {
    proto = proto[0];
    var lowerProto = proto.toLowerCase();
    this.protocol = lowerProto;
    rest = rest.slice(proto.length);
  }

  // figure out if it's got a host
  // user@server is *always* interpreted as a hostname, and url
  // resolution will treat //foo/bar as host=foo,path=bar because that's
  // how the browser resolves relative URLs.
  if (slashesDenoteHost || proto || hostPattern.test(rest)) {
    var slashes = rest.charCodeAt(0) === 47/*/*/ &&
                  rest.charCodeAt(1) === 47/*/*/;
    if (slashes && !(proto && hostlessProtocol[proto])) {
      rest = rest.slice(2);
      this.slashes = true;
    }
  }

  if (!hostlessProtocol[proto] &&
      (slashes || (proto && !slashedProtocol[proto]))) {

    // there's a hostname.
    // the first instance of /, ?, ;, or # ends the host.
    //
    // If there is an @ in the hostname, then non-host chars *are* allowed
    // to the left of the last @ sign, unless some host-ending character
    // comes *before* the @-sign.
    // URLs are obnoxious.
    //
    // ex:
    // http://a@b@c/ => user:a@b host:c
    // http://a@b?@c => user:a host:b path:/?@c

    // v0.12 TODO(isaacs): This is not quite how Chrome does things.
    // Review our test case against browsers more comprehensively.

    var hostEnd = -1;
    var atSign = -1;
    var nonHost = -1;
    for (i = 0; i < rest.length; ++i) {
      switch (rest.charCodeAt(i)) {
        case 9:   // '\t'
        case 10:  // '\n'
        case 13:  // '\r'
        case 32:  // ' '
        case 34:  // '"'
        case 37:  // '%'
        case 39:  // '\''
        case 59:  // ';'
        case 60:  // '<'
        case 62:  // '>'
        case 92:  // '\\'
        case 94:  // '^'
        case 96:  // '`'
        case 123: // '{'
        case 124: // '|'
        case 125: // '}'
          // Characters that are never ever allowed in a hostname from RFC 2396
          if (nonHost === -1)
            nonHost = i;
          break;
        case 35: // '#'
        case 47: // '/'
        case 63: // '?'
          // Find the first instance of any host-ending characters
          if (nonHost === -1)
            nonHost = i;
          hostEnd = i;
          break;
        case 64: // '@'
          // At this point, either we have an explicit point where the
          // auth portion cannot go past, or the last @ char is the decider.
          atSign = i;
          nonHost = -1;
          break;
      }
      if (hostEnd !== -1)
        break;
    }
    start = 0;
    if (atSign !== -1) {
      this.auth = decodeURIComponent(rest.slice(0, atSign));
      start = atSign + 1;
    }
    if (nonHost === -1) {
      this.host = rest.slice(start);
      rest = '';
    } else {
      this.host = rest.slice(start, nonHost);
      rest = rest.slice(nonHost);
    }

    // pull out port.
    this.parseHost();

    // we've indicated that there is a hostname,
    // so even if it's empty, it has to be present.
    if (typeof this.hostname !== 'string')
      this.hostname = '';

    var hostname = this.hostname;

    // if hostname begins with [ and ends with ]
    // assume that it's an IPv6 address.
    var ipv6Hostname = hostname.charCodeAt(0) === 91/*[*/ &&
                       hostname.charCodeAt(hostname.length - 1) === 93/*]*/;

    // validate a little.
    if (!ipv6Hostname) {
      const result = validateHostname(this, rest, hostname);
      if (result !== undefined)
        rest = result;
    }

    if (this.hostname.length > hostnameMaxLen) {
      this.hostname = '';
    } else {
      // hostnames are always lower case.
      this.hostname = this.hostname.toLowerCase();
    }

    if (!ipv6Hostname) {
      // IDNA Support: Returns a punycoded representation of "domain".
      // It only converts parts of the domain name that
      // have non-ASCII characters, i.e. it doesn't matter if
      // you call it with a domain that already is ASCII-only.

      // Use lenient mode (`true`) to try to support even non-compliant
      // URLs.
      this.hostname = toASCII(this.hostname, true);
    }

    var p = this.port ? ':' + this.port : '';
    var h = this.hostname || '';
    this.host = h + p;

    // strip [ and ] from the hostname
    // the host field still retains them, though
    if (ipv6Hostname) {
      this.hostname = this.hostname.slice(1, -1);
      if (rest[0] !== '/') {
        rest = '/' + rest;
      }
    }
  }

  // now rest is set to the post-host stuff.
  // chop off any delim chars.
  if (!unsafeProtocol[lowerProto]) {
    // First, make 100% sure that any "autoEscape" chars get
    // escaped, even if encodeURIComponent doesn't think they
    // need to be.
    const result = autoEscapeStr(rest);
    if (result !== undefined)
      rest = result;
  }

  var questionIdx = -1;
  var hashIdx = -1;
  for (i = 0; i < rest.length; ++i) {
    const code = rest.charCodeAt(i);
    if (code === 35/*#*/) {
      this.hash = rest.slice(i);
      hashIdx = i;
      break;
    } else if (code === 63/*?*/ && questionIdx === -1) {
      questionIdx = i;
    }
  }

  if (questionIdx !== -1) {
    if (hashIdx === -1) {
      this.search = rest.slice(questionIdx);
      this.query = rest.slice(questionIdx + 1);
    } else {
      this.search = rest.slice(questionIdx, hashIdx);
      this.query = rest.slice(questionIdx + 1, hashIdx);
    }
    if (parseQueryString) {
      this.query = querystring.parse(this.query);
    }
  } else if (parseQueryString) {
    // no query string, but parseQueryString still requested
    this.search = '';
    this.query = Object.create(null);
  }

  const useQuestionIdx =
    questionIdx !== -1 && (hashIdx === -1 || questionIdx < hashIdx);
  const firstIdx = useQuestionIdx ? questionIdx : hashIdx;
  if (firstIdx === -1) {
    if (rest.length > 0)
      this.pathname = rest;
  } else if (firstIdx > 0) {
    this.pathname = rest.slice(0, firstIdx);
  }
  if (slashedProtocol[lowerProto] &&
      this.hostname && !this.pathname) {
    this.pathname = '/';
  }

  // to support http.request
  if (this.pathname || this.search) {
    const p = this.pathname || '';
    const s = this.search || '';
    this.path = p + s;
  }

  // finally, reconstruct the href based on what has been validated.
  this.href = this.format();
  return this;
};

function validateHostname(self, rest, hostname) {
  for (var i = 0; i < hostname.length; ++i) {
    const code = hostname.charCodeAt(i);
    const isValid = (code >= 97/*a*/ && code <= 122/*z*/) ||
                    code === 46/*.*/ ||
                    (code >= 65/*A*/ && code <= 90/*Z*/) ||
                    (code >= 48/*0*/ && code <= 57/*9*/) ||
                    code === 45/*-*/ ||
                    code === 43/*+*/ ||
                    code === 95/*_*/ ||
                    code > 127;

    // Invalid host character
    if (!isValid) {
      self.hostname = hostname.slice(0, i);
      return '/' + hostname.slice(i) + rest;
    }
  }
}

// Automatically escape all delimiters and unwise characters from RFC 2396.
// Also escape single quotes in case of an XSS attack.
// Return undefined if the string doesn't need escaping,
// otherwise return the escaped string.
function autoEscapeStr(rest) {
  var escaped = '';
  var lastEscapedPos = 0;
  for (var i = 0; i < rest.length; ++i) {
    // Manual switching is faster than using a Map/Object.
    // `escaped` contains substring up to the last escaped character.
    switch (rest.charCodeAt(i)) {
      case 9:   // '\t'
        // Concat if there are ordinary characters in the middle.
        if (i > lastEscapedPos)
          escaped += rest.slice(lastEscapedPos, i);
        escaped += '%09';
        lastEscapedPos = i + 1;
        break;
      case 10:  // '\n'
        if (i > lastEscapedPos)
          escaped += rest.slice(lastEscapedPos, i);
        escaped += '%0A';
        lastEscapedPos = i + 1;
        break;
      case 13:  // '\r'
        if (i > lastEscapedPos)
          escaped += rest.slice(lastEscapedPos, i);
        escaped += '%0D';
        lastEscapedPos = i + 1;
        break;
      case 32:  // ' '
        if (i > lastEscapedPos)
          escaped += rest.slice(lastEscapedPos, i);
        escaped += '%20';
        lastEscapedPos = i + 1;
        break;
      case 34:  // '"'
        if (i > lastEscapedPos)
          escaped += rest.slice(lastEscapedPos, i);
        escaped += '%22';
        lastEscapedPos = i + 1;
        break;
      case 39:  // '\''
        if (i > lastEscapedPos)
          escaped += rest.slice(lastEscapedPos, i);
        escaped += '%27';
        lastEscapedPos = i + 1;
        break;
      case 60:  // '<'
        if (i > lastEscapedPos)
          escaped += rest.slice(lastEscapedPos, i);
        escaped += '%3C';
        lastEscapedPos = i + 1;
        break;
      case 62:  // '>'
        if (i > lastEscapedPos)
          escaped += rest.slice(lastEscapedPos, i);
        escaped += '%3E';
        lastEscapedPos = i + 1;
        break;
      case 92:  // '\\'
        if (i > lastEscapedPos)
          escaped += rest.slice(lastEscapedPos, i);
        escaped += '%5C';
        lastEscapedPos = i + 1;
        break;
      case 94:  // '^'
        if (i > lastEscapedPos)
          escaped += rest.slice(lastEscapedPos, i);
        escaped += '%5E';
        lastEscapedPos = i + 1;
        break;
      case 96:  // '`'
        if (i > lastEscapedPos)
          escaped += rest.slice(lastEscapedPos, i);
        escaped += '%60';
        lastEscapedPos = i + 1;
        break;
      case 123: // '{'
        if (i > lastEscapedPos)
          escaped += rest.slice(lastEscapedPos, i);
        escaped += '%7B';
        lastEscapedPos = i + 1;
        break;
      case 124: // '|'
        if (i > lastEscapedPos)
          escaped += rest.slice(lastEscapedPos, i);
        escaped += '%7C';
        lastEscapedPos = i + 1;
        break;
      case 125: // '}'
        if (i > lastEscapedPos)
          escaped += rest.slice(lastEscapedPos, i);
        escaped += '%7D';
        lastEscapedPos = i + 1;
        break;
    }
  }
  if (lastEscapedPos === 0)  // Nothing has been escaped.
    return;
  // There are ordinary characters at the end.
  if (lastEscapedPos < rest.length)
    return escaped + rest.slice(lastEscapedPos);
  else  // The last character is escaped.
    return escaped;
}

// format a parsed object into a url string
function urlFormat(obj, options) {
  // ensure it's an object, and not a string url.
  // If it's an obj, this is a no-op.
  // this way, you can call url_format() on strings
  // to clean up potentially wonky urls.
  if (typeof obj === 'string') {
    obj = urlParse(obj);
  } else if (typeof obj !== 'object' || obj === null) {
    throw new TypeError('Parameter "urlObj" must be an object, not ' +
                        (obj === null ? 'null' : typeof obj));
  } else if (!(obj instanceof Url)) {
    var format = obj[formatSymbol];
    return format ?
      format.call(obj, options) :
      Url.prototype.format.call(obj);
  }
  return obj.format();
}

Url.prototype.format = function format() {
  var auth = this.auth || '';
  if (auth) {
    auth = encodeAuth(auth);
    auth += '@';
  }

  var protocol = this.protocol || '';
  var pathname = this.pathname || '';
  var hash = this.hash || '';
  var host = '';
  var query = '';

  if (this.host) {
    host = auth + this.host;
  } else if (this.hostname) {
    host = auth + (
      this.hostname.indexOf(':') === -1 ?
        this.hostname :
        '[' + this.hostname + ']'
    );
    if (this.port) {
      host += ':' + this.port;
    }
  }

  if (this.query !== null && typeof this.query === 'object')
    query = querystring.stringify(this.query);

  var search = this.search || (query && ('?' + query)) || '';

  if (protocol && protocol.charCodeAt(protocol.length - 1) !== 58/*:*/)
    protocol += ':';

  var newPathname = '';
  var lastPos = 0;
  for (var i = 0; i < pathname.length; ++i) {
    switch (pathname.charCodeAt(i)) {
      case 35: // '#'
        if (i - lastPos > 0)
          newPathname += pathname.slice(lastPos, i);
        newPathname += '%23';
        lastPos = i + 1;
        break;
      case 63: // '?'
        if (i - lastPos > 0)
          newPathname += pathname.slice(lastPos, i);
        newPathname += '%3F';
        lastPos = i + 1;
        break;
    }
  }
  if (lastPos > 0) {
    if (lastPos !== pathname.length)
      pathname = newPathname + pathname.slice(lastPos);
    else
      pathname = newPathname;
  }

  // only the slashedProtocols get the //.  Not mailto:, xmpp:, etc.
  // unless they had them to begin with.
  if (this.slashes || slashedProtocol[protocol]) {
    if (this.slashes || host) {
      if (pathname && pathname.charCodeAt(0) !== 47/*/*/)
        pathname = '/' + pathname;
      host = '//' + host;
    } else if (protocol.length >= 4 &&
               protocol.charCodeAt(0) === 102/*f*/ &&
               protocol.charCodeAt(1) === 105/*i*/ &&
               protocol.charCodeAt(2) === 108/*l*/ &&
               protocol.charCodeAt(3) === 101/*e*/) {
      host = '//';
    }
  }

  search = search.replace(/#/g, '%23');

  if (hash && hash.charCodeAt(0) !== 35/*#*/) hash = '#' + hash;
  if (search && search.charCodeAt(0) !== 63/*?*/) search = '?' + search;

  return protocol + host + pathname + search + hash;
};

function urlResolve(source, relative) {
  return urlParse(source, false, true).resolve(relative);
}

Url.prototype.resolve = function resolve(relative) {
  return this.resolveObject(urlParse(relative, false, true)).format();
};

function urlResolveObject(source, relative) {
  if (!source) return relative;
  return urlParse(source, false, true).resolveObject(relative);
}

Url.prototype.resolveObject = function resolveObject(relative) {
  if (typeof relative === 'string') {
    var rel = new Url();
    rel.parse(relative, false, true);
    relative = rel;
  }

  var result = new Url();
  var tkeys = Object.keys(this);
  for (var tk = 0; tk < tkeys.length; tk++) {
    var tkey = tkeys[tk];
    result[tkey] = this[tkey];
  }

  // hash is always overridden, no matter what.
  // even href="" will remove it.
  result.hash = relative.hash;

  // if the relative url is empty, then there's nothing left to do here.
  if (relative.href === '') {
    result.href = result.format();
    return result;
  }

  // hrefs like //foo/bar always cut to the protocol.
  if (relative.slashes && !relative.protocol) {
    // take everything except the protocol from relative
    var rkeys = Object.keys(relative);
    for (var rk = 0; rk < rkeys.length; rk++) {
      var rkey = rkeys[rk];
      if (rkey !== 'protocol')
        result[rkey] = relative[rkey];
    }

    //urlParse appends trailing / to urls like http://www.example.com
    if (slashedProtocol[result.protocol] &&
        result.hostname && !result.pathname) {
      result.path = result.pathname = '/';
    }

    result.href = result.format();
    return result;
  }

  if (relative.protocol && relative.protocol !== result.protocol) {
    // if it's a known url protocol, then changing
    // the protocol does weird things
    // first, if it's not file:, then we MUST have a host,
    // and if there was a path
    // to begin with, then we MUST have a path.
    // if it is file:, then the host is dropped,
    // because that's known to be hostless.
    // anything else is assumed to be absolute.
    if (!slashedProtocol[relative.protocol]) {
      var keys = Object.keys(relative);
      for (var v = 0; v < keys.length; v++) {
        var k = keys[v];
        result[k] = relative[k];
      }
      result.href = result.format();
      return result;
    }

    result.protocol = relative.protocol;
    if (!relative.host &&
        !/^file:?$/.test(relative.protocol) &&
        !hostlessProtocol[relative.protocol]) {
      const relPath = (relative.pathname || '').split('/');
      while (relPath.length && !(relative.host = relPath.shift()));
      if (!relative.host) relative.host = '';
      if (!relative.hostname) relative.hostname = '';
      if (relPath[0] !== '') relPath.unshift('');
      if (relPath.length < 2) relPath.unshift('');
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

  var isSourceAbs = (result.pathname && result.pathname.charAt(0) === '/');
  var isRelAbs = (
    relative.host || relative.pathname && relative.pathname.charAt(0) === '/'
  );
  var mustEndAbs = (isRelAbs || isSourceAbs ||
                    (result.host && relative.pathname));
  var removeAllDots = mustEndAbs;
  var srcPath = result.pathname && result.pathname.split('/') || [];
  var relPath = relative.pathname && relative.pathname.split('/') || [];
  var noLeadingSlashes = result.protocol && !slashedProtocol[result.protocol];

  // if the url is a non-slashed url, then relative
  // links like ../.. should be able
  // to crawl up to the hostname, as well.  This is strange.
  // result.protocol has already been set by now.
  // Later on, put the first path part into the host field.
  if (noLeadingSlashes) {
    result.hostname = '';
    result.port = null;
    if (result.host) {
      if (srcPath[0] === '') srcPath[0] = result.host;
      else srcPath.unshift(result.host);
    }
    result.host = '';
    if (relative.protocol) {
      relative.hostname = null;
      relative.port = null;
      result.auth = null;
      if (relative.host) {
        if (relPath[0] === '') relPath[0] = relative.host;
        else relPath.unshift(relative.host);
      }
      relative.host = null;
    }
    mustEndAbs = mustEndAbs && (relPath[0] === '' || srcPath[0] === '');
  }

  if (isRelAbs) {
    // it's absolute.
    if (relative.host || relative.host === '') {
      if (result.host !== relative.host) result.auth = null;
      result.host = relative.host;
      result.port = relative.port;
    }
    if (relative.hostname || relative.hostname === '') {
      if (result.hostname !== relative.hostname) result.auth = null;
      result.hostname = relative.hostname;
    }
    result.search = relative.search;
    result.query = relative.query;
    srcPath = relPath;
    // fall through to the dot-handling below.
  } else if (relPath.length) {
    // it's relative
    // throw away the existing file, and take the new path instead.
    if (!srcPath) srcPath = [];
    srcPath.pop();
    srcPath = srcPath.concat(relPath);
    result.search = relative.search;
    result.query = relative.query;
  } else if (relative.search !== null && relative.search !== undefined) {
    // just pull out the search.
    // like href='?foo'.
    // Put this after the other two cases because it simplifies the booleans
    if (noLeadingSlashes) {
      result.hostname = result.host = srcPath.shift();
      //occasionally the auth can get stuck only in host
      //this especially happens in cases like
      //url.resolveObject('mailto:local1@domain1', 'local2@domain2')
      const authInHost =
        result.host && result.host.indexOf('@') > 0 && result.host.split('@');
      if (authInHost) {
        result.auth = authInHost.shift();
        result.host = result.hostname = authInHost.shift();
      }
    }
    result.search = relative.search;
    result.query = relative.query;
    //to support http.request
    if (result.pathname !== null || result.search !== null) {
      result.path = (result.pathname ? result.pathname : '') +
                    (result.search ? result.search : '');
    }
    result.href = result.format();
    return result;
  }

  if (!srcPath.length) {
    // no path at all.  easy.
    // we've already handled the other stuff above.
    result.pathname = null;
    //to support http.request
    if (result.search) {
      result.path = '/' + result.search;
    } else {
      result.path = null;
    }
    result.href = result.format();
    return result;
  }

  // if a url ENDs in . or .., then it must get a trailing slash.
  // however, if it ends in anything else non-slashy,
  // then it must NOT get a trailing slash.
  var last = srcPath.slice(-1)[0];
  var hasTrailingSlash = (
    (result.host || relative.host || srcPath.length > 1) &&
    (last === '.' || last === '..') || last === '');

  // strip single dots, resolve double dots to parent dir
  // if the path tries to go above the root, `up` ends up > 0
  var up = 0;
  for (var i = srcPath.length - 1; i >= 0; i--) {
    last = srcPath[i];
    if (last === '.') {
      spliceOne(srcPath, i);
    } else if (last === '..') {
      spliceOne(srcPath, i);
      up++;
    } else if (up) {
      spliceOne(srcPath, i);
      up--;
    }
  }

  // if the path is allowed to go above the root, restore leading ..s
  if (!mustEndAbs && !removeAllDots) {
    for (; up--; up) {
      srcPath.unshift('..');
    }
  }

  if (mustEndAbs && srcPath[0] !== '' &&
      (!srcPath[0] || srcPath[0].charAt(0) !== '/')) {
    srcPath.unshift('');
  }

  if (hasTrailingSlash && (srcPath.join('/').substr(-1) !== '/')) {
    srcPath.push('');
  }

  var isAbsolute = srcPath[0] === '' ||
      (srcPath[0] && srcPath[0].charAt(0) === '/');

  // put the host back
  if (noLeadingSlashes) {
    result.hostname =
      result.host = isAbsolute ? '' : srcPath.length ? srcPath.shift() : '';
    //occasionally the auth can get stuck only in host
    //this especially happens in cases like
    //url.resolveObject('mailto:local1@domain1', 'local2@domain2')
    const authInHost = result.host && result.host.indexOf('@') > 0 ?
      result.host.split('@') : false;
    if (authInHost) {
      result.auth = authInHost.shift();
      result.host = result.hostname = authInHost.shift();
    }
  }

  mustEndAbs = mustEndAbs || (result.host && srcPath.length);

  if (mustEndAbs && !isAbsolute) {
    srcPath.unshift('');
  }

  if (!srcPath.length) {
    result.pathname = null;
    result.path = null;
  } else {
    result.pathname = srcPath.join('/');
  }

  //to support request.http
  if (result.pathname !== null || result.search !== null) {
    result.path = (result.pathname ? result.pathname : '') +
                  (result.search ? result.search : '');
  }
  result.auth = relative.auth || result.auth;
  result.slashes = result.slashes || relative.slashes;
  result.href = result.format();
  return result;
};

Url.prototype.parseHost = function parseHost() {
  var host = this.host;
  var port = portPattern.exec(host);
  if (port) {
    port = port[0];
    if (port !== ':') {
      this.port = port.slice(1);
    }
    host = host.slice(0, host.length - port.length);
  }
  if (host) this.hostname = host;
};

// About 1.5x faster than the two-arg version of Array#splice().
function spliceOne(list, index) {
  for (var i = index, k = i + 1, n = list.length; k < n; i += 1, k += 1)
    list[i] = list[k];
  list.pop();
}

// These characters do not need escaping:
// ! - . _ ~
// ' ( ) * :
// digits
// alpha (uppercase)
// alpha (lowercase)
const noEscapeAuth = [
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x00 - 0x0F
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 0x10 - 0x1F
  0, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, // 0x20 - 0x2F
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, // 0x30 - 0x3F
  0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x40 - 0x4F
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 1, // 0x50 - 0x5F
  0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // 0x60 - 0x6F
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0  // 0x70 - 0x7F
];

function encodeAuth(str) {
  // faster encodeURIComponent alternative for encoding auth uri components
  var out = '';
  var lastPos = 0;
  for (var i = 0; i < str.length; ++i) {
    var c = str.charCodeAt(i);

    // ASCII
    if (c < 0x80) {
      if (noEscapeAuth[c] === 1)
        continue;
      if (lastPos < i)
        out += str.slice(lastPos, i);
      lastPos = i + 1;
      out += hexTable[c];
      continue;
    }

    if (lastPos < i)
      out += str.slice(lastPos, i);

    // Multi-byte characters ...
    if (c < 0x800) {
      lastPos = i + 1;
      out += hexTable[0xC0 | (c >> 6)] + hexTable[0x80 | (c & 0x3F)];
      continue;
    }
    if (c < 0xD800 || c >= 0xE000) {
      lastPos = i + 1;
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
    lastPos = i + 1;
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

module.exports = {
  // Original API
  Url,
  parse: urlParse,
  resolve: urlResolve,
  resolveObject: urlResolveObject,
  format: urlFormat,

  // WHATWG API
  URL,
  URLSearchParams,
  domainToASCII,
  domainToUnicode
};
