/*
Misc differences with joyent/http-parser:
  * Folding whitespace behavior conformant with RFC 7230:
      "A user agent that receives an obs-fold in a response message that is
       not within a message/http container MUST replace each received
       obs-fold with one or more SP octets prior to interpreting the field
       value."

  * Optional whitespace is removed before interpreting a header field value, as
    suggested by RFC 7230:
      "A field value might be preceded and/or followed by optional
       whitespace (OWS); a single SP preceding the field-value is preferred
       for consistent readability by humans.  The field value does not
       include any leading or trailing whitespace: OWS occurring before the
       first non-whitespace octet of the field value or after the last
       non-whitespace octet of the field value ought to be excluded by
       parsers when extracting the field value from a header field."

  * Enforces CRLF for line endings instead of additionally allowing just LF

  * Does not allow spaces in header field names

  * Smaller max chunk/content length size (double vs uint64)

  * No special handling for "Proxy-Connection" header. The reason for this is
    that "Proxy-Connection" was an experimental header for HTTP 1.0 user agents
    that ended up being a bad idea because of the confusion it can bring.
*/

var inspect = require('util').inspect;

var CR = 13;
var LF = 10;

var MAX_CHUNK_SIZE = Number.MAX_SAFE_INTEGER; // 9007199254740991

var UNHEX = {
  48: 0,
  49: 1,
  50: 2,
  51: 3,
  52: 4,
  53: 5,
  54: 6,
  55: 7,
  56: 8,
  57: 9,
  65: 10,
  66: 11,
  67: 12,
  68: 13,
  69: 14,
  70: 15,
  97: 10,
  98: 11,
  99: 12,
  100: 13,
  101: 14,
  102: 15
};

// RFC 7230 recommends at least 8000 max bytes for request line, but no
// recommendation for status lines
var MAX_HEADER_BYTES = 80 * 1024;

var RE_CLOSE = /close/i;
var RE_KEEPALIVE = /keep\-alive/i;
var RE_UPGRADE = /upgrade/i;
var RE_CHUNKED = /chunked/i;
var CC_CONNECT = 'connect'.split('').map(getFirstCharCode);
var CC_CONNECTION = 'connection'.split('').map(getFirstCharCode);
var CC_XFERENC = 'transfer-encoding'.split('').map(getFirstCharCode);
var CC_UPGRADE = 'upgrade'.split('').map(getFirstCharCode);
var CC_CONTLEN = 'content-length'.split('').map(getFirstCharCode);

// URI-parsing Regular Expressions ...

// Note: double quotes are not allowed anywhere in request URIs, but
// joyent/http-parser allowed it previously so we do too for better backwards
// compatibility ...
// Note: non-ASCII characters are not allowed anywhere in request URIs, but
// joyent/http-parser allowed it previously so we do too for better backwards
// compatibility ...
var RE_PCHAR = /(?:[A-Za-z0-9\-._~!$&'()*+,;=:@"\x80-\xFF]|%[0-9A-Fa-f]{2})/;
var RE_ABS_PATH = new RegExp('(?:/' + RE_PCHAR.source + '*)+');
// Note: double quotes are not allowed anywhere in request URIs, but
// joyent/http-parser allowed it previously so we do too for better backwards
// compatibility ...
// Note: non-ASCII characters are not allowed anywhere in request URIs, but
// joyent/http-parser allowed it previously so we do too for better backwards
// compatibility ...
var RE_QUERY = /(?:[A-Za-z0-9\-._~!$&'()*+,;=:@/?"\x80-\xFF]|%[0-9A-Fa-f]{2})*/;
// Note: fragments are technically not allowed in the request line, but
// joyent/http-parser allowed it previously so we do too for better backwards
// compatibility ...
var RE_ORIGIN_FORM = new RegExp('(?:' + RE_ABS_PATH.source + '(?:\\?' +
                                RE_QUERY.source + ')?(?:#' + RE_QUERY.source +
                                ')?)');

var RE_SCHEME = /[A-Za-z][A-Za-z0-9+\-.]*/;
var RE_USERINFO = /(?:[A-Za-z0-9\-._~!$&'()*+,;=:]|%[0-9A-Fa-f]{2})*/;
var RE_IPV4_OCTET = /(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)/;
var RE_IPV4 = new RegExp('(?:' + RE_IPV4_OCTET.source + '\\.){3}' +
                         RE_IPV4_OCTET.source);
var RE_H16 = /[0-9A-Fa-f]{1,4}/;
var RE_LS32 = new RegExp('(?:' + RE_H16.source + ':' + RE_H16.source + ')|(?:' +
                         RE_IPV4.source + ')');
var RE_H16_COLON = new RegExp('(?:' + RE_H16.source + ':)');
var RE_IPV6 = new RegExp('(?:' +
    // Begin LS32 postfix cases
    '(?:' +
    [
      RE_H16_COLON.source + '{6}',
      '::' + RE_H16_COLON.source + '{5}',
      '(?:' + RE_H16.source + ')?::' + RE_H16_COLON.source + '{4}',
      '(?:' + RE_H16_COLON.source + '{0,1}' + RE_H16.source + ')?::' +
          RE_H16_COLON.source + '{3}',
      '(?:' + RE_H16_COLON.source + '{0,2}' + RE_H16.source + ')?::' +
          RE_H16_COLON.source + '{2}',
      '(?:' + RE_H16_COLON.source + '{0,3}' + RE_H16.source + ')?::' +
          RE_H16_COLON.source,
      '(?:' + RE_H16_COLON.source + '{0,4}' + RE_H16.source + ')?::',
    ].join(')|(?:') +
    ')(?:' + RE_LS32.source + ')' +
    // End LS32 postfix cases
    ')' +
    '|(?:(?:' + RE_H16_COLON.source + '{0,5}' + RE_H16.source + ')?::' +
        RE_H16.source + ')' +
    '|(?:(?:' + RE_H16_COLON.source + '{0,6}' + RE_H16.source + ')?::)');
var RE_REGNAME = /(?:[A-Za-z0-9\-._~!$&'()*+,;=]|%[0-9A-Fa-f]{2})*/;
var RE_HOST = new RegExp('(?:(?:\\[' + RE_IPV6.source + '\\])|(?:' +
                         RE_IPV4.source + ')|' + RE_REGNAME.source + ')');
var RE_AUTHORITY = new RegExp('(?:(?:' + RE_USERINFO.source + '@)?' +
                              RE_HOST.source + '(?::[0-9]*)?)');
var RE_PATH_ABEMPTY = new RegExp('(?:/' + RE_PCHAR.source + '*)*');
var RE_PATH_ROOTLESS = new RegExp('(?:' + RE_PCHAR.source + '+' +
                                  RE_PATH_ABEMPTY.source + ')');
var RE_PATH_ABSOLUTE = new RegExp('(?:/' + RE_PATH_ROOTLESS.source + '?)');
var RE_HIER_PART = new RegExp('(?:(?://' + RE_AUTHORITY.source +
                              RE_PATH_ABEMPTY.source + ')|' +
                              RE_PATH_ABSOLUTE.source + '|' +
                              RE_PATH_ROOTLESS.source + '|)');
// Note: fragments are technically not allowed in the request line, but
// joyent/http-parser allowed it previously so we do too for better backwards
// compatibility ...
var RE_ABSOLUTE_FORM = new RegExp('(?:' + RE_SCHEME.source + ':' +
                                  RE_HIER_PART.source + '(?:\\?' +
                                  RE_QUERY.source + ')?(?:#' +
                                  RE_QUERY.source + ')?)');

var RE_REQUEST_TARGET = new RegExp('(?:' + RE_ORIGIN_FORM.source + '|' +
                                   RE_ABSOLUTE_FORM.source + '|' +
                                   RE_AUTHORITY.source + '|\\*)');
var RE_REQUEST_LINE = new RegExp('^([!#$%\'*+\\-.^_`|~0-9A-Za-z]+) (' +
                                 RE_REQUEST_TARGET.source +
                                 ')(?: HTTP\\/1\\.([01]))?$');
/*
request-target              = origin-form | absolute-form | authority-form |
                              asterisk-form
  origin-form               = absolute-path [ "?" query ]
    absolute-path           = 1*( "/" segment )
      segment               = *pchar
        pchar               = unreserved | pct-encoded | sub-delims | ":" | "@"
          unreserved        = ALPHA | DIGIT | "-" | "." | "_" | "~"
          pct-encoded       = "%" HEXDIG HEXDIG
            HEXDIG          = DIGIT | "A" | "B" | "C" | "D" | "E" | "F"
          sub-delims        = "!" | "$" | "&" | "'" | "(" | ")" | "*" | "+" |
                              "," | ";" | "="
    query                   = *( pchar | "/" | "?" )
  absolute-form             = absolute-URI
    absolute-URI            = scheme ":" hier-part [ "?" query ]
      scheme                = alpha *( alpha | digit | "+" | "-" | "." )
      hier-part             = "//" authority path-abempty | path-absolute |
                              path-rootless | path-empty
        authority           = [ userinfo "@" ] host [ ":" port ]
          userinfo          = *( unreserved | pct-encoded | sub-delims | ":" )
          host              = IP-literal | IPv4address | reg-name
            IP-literal      = "[" ( IPv6address | IPvFuture ) "]"
              IPv6address   =                               6( h16 ":" ) ls32
                               |                       "::" 5( h16 ":" ) ls32
                               | [               h16 ] "::" 4( h16 ":" ) ls32
                               | [ *1( h16 ":" ) h16 ] "::" 3( h16 ":" ) ls32
                               | [ *2( h16 ":" ) h16 ] "::" 2( h16 ":" ) ls32
                               | [ *3( h16 ":" ) h16 ] "::"    h16 ":"   ls32
                               | [ *4( h16 ":" ) h16 ] "::"              ls32
                               | [ *5( h16 ":" ) h16 ] "::"              h16
                               | [ *6( h16 ":" ) h16 ] "::"
                h16         = 1*4HEXDIG
                ls32        = ( h16 ":" h16 ) | IPv4address
                IPv4address = dec-octet "." dec-octet "." dec-octet "."
                              dec-octet
                dec-octet   =   DIGIT                 ; 0-9
                              | %x31-39 DIGIT         ; 10-99
                              | "1" 2DIGIT            ; 100-199
                              | "2" %x30-34 DIGIT     ; 200-249
                              | "25" %x30-35          ; 250-255
            reg-name        = *( unreserved | pct-encoded | sub-delims )
          port              = *DIGIT
        path-abempty        = *( "/" segment )
        path-absolute       = "/" [ segment-nz *( "/" segment ) ]
          segment-nz        = 1*pchar
        path-rootless       = segment-nz *( "/" segment )
        path-empty          = 0<pchar>
  authority-form            = authority
  asterisk-form             = "*"
*/

// Note: AT LEAST a space is technically required after the status code, but
// joyent/http-parser allows a CRLF immediately following the status code, so we
// do also for backwards compatibility ...
var RE_STATUS_LINE = /^HTTP\/1\.([01]) ([0-9]{3})(?: (.*))?$/;

var RE_HEADER = /^([!#$%'*+\-.^_`|~0-9A-Za-z]+):(.*)$/;

var STATE_REQ_LINE = 0;
var STATE_STATUS_LINE = 1;
var STATE_HEADER = 2;
var STATE_BODY_LITERAL = 3;
var STATE_BODY_EOF = 4;
var STATE_BODY_CHUNKED_SIZE = 5;
var STATE_BODY_CHUNKED_BYTES = 6;
var STATE_BODY_CHUNKED_BYTES_CRLF = 7;
var STATE_COMPLETE = 8;
var STATE_NAMES = [
  'STATE_REQ_LINE',
  'STATE_STATUS_LINE',
  'STATE_HEADER',
  'STATE_BODY_LITERAL',
  'STATE_BODY_EOF',
  'STATE_BODY_CHUNKED_SIZE',
  'STATE_BODY_CHUNKED_BYTES',
  'STATE_BODY_CHUNKED_BYTES_CRLF',
  'STATE_COMPLETE'
];

var FLAG_CHUNKED = 1 << 0;
var FLAG_CONNECTION_KEEP_ALIVE = 1 << 1;
var FLAG_CONNECTION_CLOSE = 1 << 2;
var FLAG_CONNECTION_UPGRADE = 1 << 3;
var FLAG_TRAILING = 1 << 4;
var FLAG_UPGRADE = 1 << 5;
var FLAG_SKIPBODY = 1 << 6;
var FLAG_ANY_UPGRADE = FLAG_UPGRADE | FLAG_CONNECTION_UPGRADE;

function HTTPParser(type) {
  this.onHeaders = undefined;
  this.onBody = undefined;
  this.onComplete = undefined;

  // extra stuff tagged onto parser object by core modules/files
  this.onIncoming = undefined;
  this.incoming = undefined;
  this.socket = undefined;

  this.reinitialize(type);
}
HTTPParser.prototype.reinitialize = function(type) {
  this.execute = this._executeStartLine;
  this.type = type;
  if (type === HTTPParser.REQUEST)
    this._state = STATE_REQ_LINE;
  else
    this._state = STATE_STATUS_LINE;

  this._err = undefined;
  this._flags = 0;
  this._contentLen = undefined;
  this._nbytes = 0;
  this._nhdrbytes = 0;
  this._nhdrpairs = 0;
  this._buf = '';
  this._seenCR = false;

  // common properties
  this.headers = [];
  this.httpMajor = 1;
  this.httpMinor = undefined;
  this.maxHeaderPairs = 2000;

  // request properties
  this.method = undefined;
  this.url = undefined;

  // response properties
  this.statusCode = undefined;
  this.statusText = undefined;
};
HTTPParser.prototype.finish = function() {
  var state = this._state;
  if (state === STATE_BODY_EOF) {
    this.execute = this._executeBodyIgnore;
    this._state = STATE_COMPLETE;
    this.onComplete && this.onComplete();
  } else if (this.execute !== this._executeError &&
             state !== STATE_REQ_LINE &&
             state !== STATE_STATUS_LINE &&
             state !== STATE_COMPLETE) {
    return this._setError('Invalid EOF state');
  }
};
HTTPParser.prototype.close = function() {};
HTTPParser.prototype.pause = function() {};
HTTPParser.prototype.resume = function() {};
HTTPParser.prototype._setError = function(msg) {
  var err = new Error(msg);
  this.execute = this._executeError;
  this._err = err;
  return err;
};
HTTPParser.prototype._processHdrLine = function(line) {
  switch (this._state) {
    case STATE_HEADER:
      if (line.length === 0) {
        // We saw a double CRLF
        this._headersEnd();
        return;
      }
      var headers = this.headers;
      var headerslen = headers.length;
      var fieldName;
      var fieldValue;
      var m = RE_HEADER.exec(line);
      if (m === null) {
        var firstChr = line.charCodeAt(0);
        if (firstChr !== 32 & firstChr !== 9)
          return this._setError('Malformed header line');
        // RFC 7230 compliant, but less backwards compatible:
        var extra = ltrim(line);
        if (extra.length > 0) {
          if (headerslen === 0)
            return this._setError('Malformed header line');
          fieldName = headers[headerslen - 2];
          fieldValue = headers[headerslen - 1] + ' ' + extra;
          // Need to re-check value since matched values may now exist ...
          if (equalsLower(fieldName, CC_CONNECTION)) {
            if (fieldValue.search(RE_CLOSE) > -1)
              this._flags |= FLAG_CONNECTION_CLOSE;
            if (fieldValue.search(RE_KEEPALIVE) > -1)
              this._flags |= FLAG_CONNECTION_KEEP_ALIVE;
            if (fieldValue.search(RE_UPGRADE) > -1)
              this._flags |= FLAG_CONNECTION_UPGRADE;
          } else if (equalsLower(fieldName, CC_XFERENC)) {
            if (fieldValue.search(RE_CHUNKED) > -1)
              this._flags |= FLAG_CHUNKED;
          } else if (equalsLower(fieldName, CC_UPGRADE)) {
            this._flags |= FLAG_UPGRADE;
          } else if (equalsLower(fieldName, CC_CONTLEN)) {
            var val = parseInt(fieldValue, 10);
            if (val !== val || val > MAX_CHUNK_SIZE)
              return this._setError('Bad Content-Length: ' + inspect(val));
            this._contentLen = val;
          }
          headers[headerslen - 1] = fieldValue;
        }
      } else {
        // Ensures that trailing whitespace after the last folded line for
        // header values gets trimmed
        if (headerslen > 0)
          headers[headerslen - 1] = trim(headers[headerslen - 1]);
        // m[1]: field name
        // m[2]: field value
        fieldName = m[1];
        fieldValue = m[2];
        if (equalsLower(fieldName, CC_CONNECTION)) {
          if (fieldValue.search(RE_CLOSE) > -1)
            this._flags |= FLAG_CONNECTION_CLOSE;
          if (fieldValue.search(RE_KEEPALIVE) > -1)
            this._flags |= FLAG_CONNECTION_KEEP_ALIVE;
          if (fieldValue.search(RE_UPGRADE) > -1)
            this._flags |= FLAG_CONNECTION_UPGRADE;
        } else if (equalsLower(fieldName, CC_XFERENC)) {
          if (fieldValue.search(RE_CHUNKED) > -1)
            this._flags |= FLAG_CHUNKED;
        } else if (equalsLower(fieldName, CC_UPGRADE)) {
          this._flags |= FLAG_UPGRADE;
        } else if (equalsLower(fieldName, CC_CONTLEN)) {
          var val = parseInt(fieldValue, 10);
          if (val !== val || val > MAX_CHUNK_SIZE)
            return this._setError('Bad Content-Length: ' + inspect(val));
          this._contentLen = val;
        }
        var maxHeaderPairs = this.maxHeaderPairs;
        if (maxHeaderPairs <= 0 || ++this._nhdrpairs < maxHeaderPairs)
          headers.push(fieldName, fieldValue);
      }
      break;
    case STATE_REQ_LINE:
      // Original HTTP parser ignored blank lines before request/status line,
      // so we do that here too ...
      if (line.length === 0)
        return true;
      var m = RE_REQUEST_LINE.exec(line);
      if (m === null)
        return this._setError('Malformed request line');
      // m[1]: HTTP method
      // m[2]: request target
      // m[3]: HTTP minor version
      var method = m[1];
      var minor = m[3];
      this.method = method;
      this.url = m[2];
      if (minor === undefined) {
        // HTTP/0.9 ugh...
        if (method !== 'GET')
          return this._setError('Malformed request line');
        this.httpMajor = 0;
        this.httpMinor = 9;
        this._headersEnd();
      } else {
        this.httpMinor = parseInt(minor, 10);
        this._state = STATE_HEADER;
      }
      break;
    case STATE_STATUS_LINE:
      // Original HTTP parser ignored blank lines before request/status line,
      // so we do that here too ...
      if (line.length === 0)
        return true;
      var m = RE_STATUS_LINE.exec(line);
      if (m === null)
        return this._setError('Malformed status line');
      // m[1]: HTTP minor version
      // m[2]: HTTP status code
      // m[3]: Reason text
      this.httpMinor = parseInt(m[1], 10);
      this.statusCode = parseInt(m[2], 10);
      this.statusText = m[3] || '';
      this._state = STATE_HEADER;
      break;
    default:
      return this._setError('Unexpected HTTP parser state: ' + this._state);
  }
};
HTTPParser.prototype._headersEnd = function() {
  var flags = this._flags;
  var type = this.type;
  var method = this.method;
  var upgrade = ((flags & FLAG_ANY_UPGRADE) === FLAG_ANY_UPGRADE ||
                 (type === REQUEST && equalsLower(method, CC_CONNECT)));
  var contentLen = this._contentLen;
  var httpMajor = this.httpMajor;
  var httpMinor = this.httpMinor;
  var keepalive = this._shouldKeepAlive(httpMajor, httpMinor, flags);
  var ret;

  this._buf = '';
  this._seenCR = false;
  this._nbytes = 0;

  if ((flags & FLAG_CHUNKED) > 0) {
    this._state = STATE_BODY_CHUNKED_SIZE;
    this.execute = this._executeBodyChunked;
  } else if (contentLen !== undefined) {
    this._state = STATE_BODY_LITERAL;
    this.execute = this._executeBodyLiteral;
  } else {
    this._state = STATE_BODY_EOF;
    this.execute = this._executeBodyEOF;
  }

  var headers = this.headers;
  var headerslen = headers.length;
  if ((flags & FLAG_TRAILING) > 0) {
    if (headerslen > 0)
      headers[headerslen - 1] = trim(headers[headerslen - 1]);
    this.onComplete && this.onComplete();
    this.reinitialize(type);
    return;
  } else {
    this.headers = [];
    if (this.onHeaders) {
      if (headerslen > 0)
        headers[headerslen - 1] = trim(headers[headerslen - 1]);
      ret = this.onHeaders(httpMajor, httpMinor, headers, method,
                           this.url, this.statusCode, this.statusText, upgrade,
                           keepalive);
      if (ret === true)
        flags = (this._flags |= FLAG_SKIPBODY);
    }
  }

  if (upgrade) {
    this.onComplete && this.onComplete();
    this._state = STATE_COMPLETE;
  } else if (contentLen === 0 ||
             (flags & FLAG_SKIPBODY) > 0 ||
             ((flags & FLAG_CHUNKED) === 0 &&
              contentLen === undefined &&
              !this._needsEOF())) {
    this.onComplete && this.onComplete();
    this.reinitialize(type);
  }
};
HTTPParser.prototype._executeStartLine = function(data) {
  if (data.length === 0)
    return 0;
  var firstByte = data[0];
  if ((firstByte < 32 || firstByte >= 127) && firstByte !== CR)
    return this._setError('Invalid byte(s) in start line');
  this.execute = this._executeHeader;
  return this.execute(data);
};
HTTPParser.prototype._executeHeader = function(data) {
  var len = data.length;

  if (len === 0)
    return 0;

  var offset = 0;
  var seenCR = this._seenCR;
  var buf = this._buf;
  var nhdrbytes = this._nhdrbytes;
  var ret;

  while (offset < len) {
    if (seenCR) {
      seenCR = false;
      if (data[offset] === LF) {
        // Our internal buffer contains a full line
        ++offset;
        ret = this._processHdrLine(buf);
        buf = '';
        if (typeof ret === 'object')
          return ret;
        else if (ret === undefined) {
          var state = this._state;
          if (state !== STATE_HEADER) {
            // Begin of body or end of message
            if (state < STATE_COMPLETE && offset < len) {
              // Execute extra body bytes
              ret = this.execute(data.slice(offset));
              if (typeof ret !== 'number')
                return ret;
              return offset + ret;
            } else if (state === STATE_COMPLETE)
              this.reinitialize(this.type);
            return offset;
          }
        }
      } else {
        // False match
        buf += '\r';
        ++nhdrbytes;
        if (nhdrbytes > MAX_HEADER_BYTES) {
          return this._setError('Header size limit exceeded (' +
                                MAX_HEADER_BYTES + ')');
        }
      }
    }
    ret = indexOfCRLF(data, len, offset);
    if (ret > -1) {
      // Our internal buffer contains a full line
      var bytesToAdd = ret - offset;
      if (bytesToAdd > 0) {
        nhdrbytes += bytesToAdd;
        if (nhdrbytes > MAX_HEADER_BYTES) {
          return this._setError('Header size limit exceeded (' +
                                MAX_HEADER_BYTES + ')');
        }
        buf += data.toString('binary', offset, ret);
      }
      offset = ret + 2;
      ret = this._processHdrLine(buf);
      buf = '';
      if (typeof ret === 'object')
        return ret;
      else if (ret === undefined) {
        var state = this._state;
        if (state !== STATE_HEADER) {
          // Begin of body or end of message
          if (state < STATE_COMPLETE && offset < len) {
            // Execute extra body bytes
            ret = this.execute(data.slice(offset));
            if (typeof ret !== 'number')
              return ret;
            return offset + ret;
          } else if (state === STATE_COMPLETE)
            this.reinitialize(this.type);
          return offset;
        }
      }
    } else {
      // Check for possible cross-chunk CRLF split
      var end;
      if (data[len - 1] === CR) {
        seenCR = true;
        end = len - 1;
      } else
        end = len;

      nhdrbytes += end - offset;

      if (nhdrbytes > MAX_HEADER_BYTES) {
        return this._setError('Header size limit exceeded (' +
                              MAX_HEADER_BYTES + ')');
      }
      buf += data.toString('binary', offset, end);
      break;
    }
  }

  this._buf = buf;
  this._seenCR = seenCR;
  this._nhdrbytes = nhdrbytes;

  return len;
};
HTTPParser.prototype._executeBodyChunked = function(data) {
  var len = data.length;

  if (len === 0)
    return 0;

  var offset = 0;
  var seenCR = this._seenCR;
  var buf = this._buf;
  var nbytes = this._nbytes;
  var ret;
  var bytesToAdd;

  while (offset < len) {
    switch (this._state) {
      case STATE_BODY_CHUNKED_SIZE:
        if (seenCR) {
          seenCR = false;
          if (data[offset] === LF) {
            // Our internal buffer contains a full line
            ++offset;
            ret = readChunkSize(buf);
            buf = '';
            if (typeof ret !== 'number') {
              this.execute = this._executeError;
              this._err = ret;
              return ret;
            } else if (ret === 0) {
              this._seenCR = false;
              this._buf = '';
              this._flags |= FLAG_TRAILING;
              this._state = STATE_HEADER;
              this.execute = this._executeHeader;
              if (offset < len) {
                ret = this.execute(data.slice(offset));
                if (typeof ret !== 'number')
                  return ret;
                return offset + ret;
              }
              return offset;
            } else {
              nbytes = ret;
              this._state = STATE_BODY_CHUNKED_BYTES;
              continue;
            }
          } else {
            // False match
            buf += '\r';
          }
        }
        ret = indexOfCRLF(data, len, offset);
        if (ret > -1) {
          // Our internal buffer contains a full line
          bytesToAdd = ret - offset;
          if (bytesToAdd > 0)
            buf += data.toString('ascii', offset, ret);

          offset = ret + 2;
          ret = readChunkSize(buf);
          buf = '';

          if (typeof ret !== 'number') {
            this.execute = this._executeError;
            this._err = ret;
            return ret;
          } else if (ret === 0) {
            this._seenCR = false;
            this._buf = '';
            this._flags |= FLAG_TRAILING;
            this._state = STATE_HEADER;
            this.execute = this._executeHeader;
            if (offset < len) {
              ret = this.execute(data.slice(offset));
              if (typeof ret !== 'number')
                return ret;
              return offset + ret;
            }
            return offset;
          } else {
            nbytes = ret;
            this._state = STATE_BODY_CHUNKED_BYTES;
            continue;
          }
        } else {
          // Check for possible cross-chunk CRLF split
          var end;
          if (data[len - 1] === CR) {
            seenCR = true;
            end = len - 1;
          }
          buf += data.toString('ascii', offset, end);
          offset = len; // break out of while loop
        }
        break;
      case STATE_BODY_CHUNKED_BYTES:
        var dataleft = len - offset;
        if (dataleft >= nbytes) {
          this.onBody(data, offset, nbytes);
          offset += nbytes;
          nbytes = 0;
          this._state = STATE_BODY_CHUNKED_BYTES_CRLF;
        } else {
          nbytes -= dataleft;
          this.onBody(data, offset, len);
          offset = len;
        }
        break;
      case STATE_BODY_CHUNKED_BYTES_CRLF:
        if (nbytes === 0 && data[offset++] === CR)
          ++nbytes;
        else if (nbytes === 1 && data[offset++] === LF)
          this._state = STATE_BODY_CHUNKED_SIZE;
        else
          return this._setError('Malformed chunk (missing CRLF)');
        break;
      default:
        return this._setError('Unexpected parser state while reading chunks');
    }
  }

  this._buf = buf;
  this._seenCR = seenCR;
  this._nbytes = nbytes;

  return len;
};
HTTPParser.prototype._executeBodyLiteral = function(data) {
  var len = data.length;

  if (len === 0)
    return 0;

  var nbytes = this._contentLen;
  if (len >= nbytes) {
    this.reinitialize(this.type);
    this.onBody(data, 0, nbytes);
    this.onComplete && this.onComplete();
    return nbytes;
  } else {
    this._contentLen -= len;
    this.onBody(data, 0, len);
    return len;
  }
};
HTTPParser.prototype._executeBodyEOF = function(data) {
  var len = data.length;

  if (len === 0)
    return 0;

  this.onBody(data, 0, len);
  return len;
};
HTTPParser.prototype._executeBodyIgnore = function(data) {
  return 0;
};
HTTPParser.prototype._executeError = function(data) {
  return this._err;
};
HTTPParser.prototype.execute = HTTPParser.prototype._executeStartLine;
HTTPParser.prototype._shouldKeepAlive = function(httpMajor, httpMinor, flags) {
  if (httpMajor > 0 && httpMinor > 0) {
    if ((flags & FLAG_CONNECTION_CLOSE) > 0)
      return false;
  } else {
    if ((flags & FLAG_CONNECTION_KEEP_ALIVE) === 0)
      return false;
  }
  return !this._needsEOF();
};
HTTPParser.prototype._needsEOF = function() {
  if (this.type === HTTPParser.REQUEST)
    return false;

  // See RFC 2616 section 4.4
  var status = this.statusCode;
  var flags = this._flags;
  if ((status !== undefined &&
       (status === 204 ||                     // No Content
        status === 304 ||                     // Not Modified
        parseInt(status / 100, 10) === 1)) || // 1xx e.g. Continue
      (flags & FLAG_SKIPBODY) > 0) {          // response to a HEAD request
    return false;
  }

  if ((flags & FLAG_CHUNKED) > 0 || this._contentLen !== undefined)
    return false;

  return true;
};




var REQUEST = HTTPParser.REQUEST = 0;
var RESPONSE = HTTPParser.RESPONSE = 1;
module.exports = HTTPParser;

function indexOfCRLF(buf, buflen, offset) {
  var bo1;
  while (offset < buflen) {
    bo1 = buf[offset + 1];
    if (buf[offset] === CR && bo1 === LF)
      return offset;
    else if (bo1 === CR)
      ++offset;
    else
      offset += 2;
  }
  return -1;
}

function readChunkSize(str) {
  var val, dec;
  for (var i = 0; i < str.length; ++i) {
    dec = UNHEX[str.charCodeAt(i)];
    if (dec === undefined)
      break;
    else if (val === undefined)
      val = dec;
    else {
      val *= 16;
      val += dec;
    }
  }
  if (val === undefined)
    val = new Error('Invalid chunk size');
  else if (val > MAX_CHUNK_SIZE)
    val = new Error('Chunk size too big');
  return val;
}

function ltrim(value) {
  var length = value.length, start;
  for (start = 0;
       start < length &&
       (value.charCodeAt(start) === 32 || value.charCodeAt(start) === 9);
       ++start);
  return start > 0 ? value.slice(start) : value;
}
function trim(value) {
  var length = value.length, start, end;
  for (start = 0;
       start < length &&
       (value.charCodeAt(start) === 32 || value.charCodeAt(start) === 9);
       ++start);
  for (end = length;
       end > start &&
       (value.charCodeAt(end - 1) === 32 || value.charCodeAt(end - 1) === 9);
       --end);
  return start > 0 || end < length ? value.slice(start, end) : value;
}

function equalsLower(input, ref) {
  var inlen = input.length;
  var reflen = ref.length;
  if (inlen !== reflen)
    return false;
  for (var i = 0; i < inlen; ++i) {
    if ((input.charCodeAt(i) | 0x20) !== ref[i])
      return false;
  }
  return true;
}

function getFirstCharCode(str) {
  return str.charCodeAt(0);
}
