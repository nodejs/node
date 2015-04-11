var inspect = require('util').inspect;

var CRLF = new Buffer('\r\n');
var CR = 13;
var LF = 10;

var MAX_CHUNK_SIZE_LEN = 16; // max length of chunk size line
var MAX_CHUNK_SIZE = 9007199254740992;
// RFC 7230 recommends at least 8000 max bytes for request line, but no
// recommendation for status lines
var MAX_HEADER_BYTES = 80 * 1024;

var RE_CHUNK_LEN = /^[0-9A-Fa-f]+/;

var RE_PCHAR = /(?:[A-Za-z0-9\-._~!$&'()*+,;=:@]|%[0-9A-Fa-f]{2})/;
var RE_ABS_PATH = new RegExp('(?:/' + RE_PCHAR.source + '*)+');
var RE_QUERY = /(?:[A-Za-z0-9\-._~!$&'()*+,;=:@/?]|%[0-9A-Fa-f]{2})*/;
// Note: fragments are technically not allowed in the request line, but
// joyent/http-parser allowed it previously so we do also for backwards
// compatibility ...
var RE_ORIGIN_FORM = new RegExp('(?:' + RE_ABS_PATH.source + '(?:\\?' + RE_QUERY.source + ')?(?:#' + RE_QUERY.source + ')?)');

var RE_SCHEME = /[A-Za-z][A-Za-z0-9+\-.]*/;
var RE_USERINFO = /(?:[A-Za-z0-9\-._~!$&'()*+,;=:]|%[0-9A-Fa-f]{2})*/;
var RE_IPV4_OCTET = /(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)/;
var RE_IPV4 = new RegExp('(?:' + RE_IPV4_OCTET.source + '\\.){3}' + RE_IPV4_OCTET.source);
var RE_H16 = /[0-9A-Fa-f]{1,4}/;
var RE_LS32 = new RegExp('(?:' + RE_H16.source + ':' + RE_H16.source + ')|(?:' + RE_IPV4.source + ')');
var RE_H16_COLON = new RegExp('(?:' + RE_H16.source + ':)');
var RE_IPV6 = new RegExp('(?:'
                         // Begin LS32 postfix cases
                         + '(?:'
                         + [
                          RE_H16_COLON.source + '{6}',
                          '::' + RE_H16_COLON.source + '{5}',
                          '(?:' + RE_H16.source + ')?::' + RE_H16_COLON.source + '{4}',
                          '(?:' + RE_H16_COLON.source + '{0,1}' + RE_H16.source + ')?::' + RE_H16_COLON.source + '{3}',
                          '(?:' + RE_H16_COLON.source + '{0,2}' + RE_H16.source + ')?::' + RE_H16_COLON.source + '{2}',
                          '(?:' + RE_H16_COLON.source + '{0,3}' + RE_H16.source + ')?::' + RE_H16_COLON.source,
                          '(?:' + RE_H16_COLON.source + '{0,4}' + RE_H16.source + ')?::',
                         ].join(')|(?:')
                         + ')(?:' + RE_LS32.source + ')'
                         // End LS32 postfix cases
                         + ')'
                         + '|(?:(?:' + RE_H16_COLON.source + '{0,5}' + RE_H16.source + ')?::' + RE_H16.source + ')'
                         + '|(?:(?:' + RE_H16_COLON.source + '{0,6}' + RE_H16.source + ')?::)');

var RE_REGNAME = /(?:[A-Za-z0-9\-._~!$&'()*+,;=]|%[0-9A-Fa-f]{2})*/;
var RE_HOST = new RegExp('(?:(?:\\[' + RE_IPV6.source + '\\])|(?:' + RE_IPV4.source + ')|' + RE_REGNAME.source + ')');
var RE_AUTHORITY = new RegExp('(?:(?:' + RE_USERINFO.source + ')?' + RE_HOST.source + '(?::[0-9]*)?)');
var RE_PATH_ABEMPTY = new RegExp('(?:/' + RE_PCHAR.source + '*)*');
var RE_PATH_ROOTLESS = new RegExp('(?:' + RE_PCHAR.source + '+' + RE_PATH_ABEMPTY.source + ')');
var RE_PATH_ABSOLUTE = new RegExp('(?:/' + RE_PATH_ROOTLESS.source + '?)');
var RE_HIER_PART = new RegExp('//(?:' + RE_AUTHORITY.source + ')(?:' + RE_PATH_ABEMPTY + '|' + RE_PATH_ABSOLUTE + '|' + RE_PATH_ROOTLESS + '|)');
// Note: fragments are technically not allowed in the request line, but
// joyent/http-parser allowed it previously so we do also for backwards
// compatibility ...
var RE_ABSOLUTE_FORM = new RegExp('(?:' + RE_SCHEME.source + ':' + RE_HIER_PART.source + '(?:\\?' + RE_QUERY.source + ')?(?:#' + RE_QUERY.source + ')?)');

var RE_REQUEST_TARGET = new RegExp('(?:' + RE_ORIGIN_FORM.source + '|' + RE_ABSOLUTE_FORM.source + '|' + RE_AUTHORITY.source + '|\\*)');
var RE_REQUEST_LINE = new RegExp('^([!#$%\'*+\\-.^_`|~0-9A-Za-z]+) (' + RE_REQUEST_TARGET.source + ')(?: HTTP\\/1\\.([01]))?$');
/*
request-target              = origin-form | absolute-form | authority-form | asterisk-form
  origin-form               = absolute-path [ "?" query ]
    absolute-path           = 1*( "/" segment )
      segment               = *pchar
        pchar               = unreserved | pct-encoded | sub-delims | ":" | "@"
          unreserved        = ALPHA | DIGIT | "-" | "." | "_" | "~"
          pct-encoded       = "%" HEXDIG HEXDIG
            HEXDIG          = DIGIT | "A" | "B" | "C" | "D" | "E" | "F"
          sub-delims        = "!" | "$" | "&" | "'" | "(" | ")" | "*" | "+" | "," | ";" | "="
    query                   = *( pchar | "/" | "?" )
  absolute-form             = absolute-URI
    absolute-URI            = scheme ":" hier-part [ "?" query ]
      scheme                = alpha *( alpha | digit | "+" | "-" | "." )
      hier-part             = "//" authority path-abempty | path-absolute | path-rootless | path-empty
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
                IPv4address = dec-octet "." dec-octet "." dec-octet "." dec-octet
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
// joyent/http-parser allows a CRLF immediate following the status code, so we
// do also for backwards compatibility ...
var RE_STATUS_LINE = /^HTTP\/1\.([01]) ([0-9]{3})(?: ((?:[\x21-\x7E](?:[\t ]+[\x21-\x7E])*)*))?$/;

var RE_HEADER = /^([!#$%'*+\-.^_`|~0-9A-Za-z]+):[\t ]*((?:[\x21-\x7E](?:[\t ]+[\x21-\x7E])*)*)[\t ]*$/;
var RE_FOLDED = /^[\t ]+(.*)$/;

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
  this.execute = this._executeHeader;
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
  if (this._state === STATE_BODY_EOF) {
    this.execute = this._executeBodyIgnore;
    this._state = STATE_COMPLETE;
    this.onComplete && this.onComplete();
  }
};
HTTPParser.prototype.close = function() {};
HTTPParser.prototype.pause = function() {};
HTTPParser.prototype.resume = function() {};
HTTPParser.prototype._processHdrLine = function(line) {
  switch (this._state) {
    case STATE_HEADER:
      if (line.length === 0) {
        // We saw a double CRLF
        this._headersEnd();
        return;
      }
      var m = RE_HEADER.exec(line);
      if (m === null) {
        m = RE_FOLDED.exec(line);
        if (m === null) {
          this.execute = this._executeError;
          this._err = new Error('Malformed header line');
          return this._err;
        }
        var extra = m[1];
        if (extra.length > 0) {
          var headers = this.headers;
          headers[headers.length - 1] += ' ' + extra;
        }
      } else {
        // m[1]: field name
        // m[2]: field value
        var fieldName = m[1];
        var fieldValue = m[2];
        switch (fieldName.toLowerCase()) {
          case 'connection':
            var valLower = fieldValue.toLowerCase();
            if (valLower.substring(0, 5) === 'close')
              this._flags |= FLAG_CONNECTION_CLOSE;
            else if (valLower.substring(0, 10) === 'keep-alive')
              this._flags |= FLAG_CONNECTION_KEEP_ALIVE;
            else if (valLower.substring(0, 7) === 'upgrade')
              this._flags |= FLAG_CONNECTION_UPGRADE;
          break;
          case 'transfer-encoding':
            var valLower = fieldValue.toLowerCase();
            if (valLower.substring(0, 7) === 'chunked')
              this._flags |= FLAG_CHUNKED;
          break;
          case 'upgrade':
            this._flags |= FLAG_UPGRADE;
          break;
          case 'content-length':
            var val = parseInt(fieldValue, 10);
            if (isNaN(val) || val > MAX_CHUNK_SIZE) {
              this.execute = this._executeError;
              this._err = new Error('Bad Content-Length: ' + inspect(val));
              return this._err;
            }
            this._contentLen = val;
          break;
        }
        var maxHeaderPairs = this.maxHeaderPairs;
        if (maxHeaderPairs <= 0 || ++this._nhdrpairs < maxHeaderPairs)
          this.headers.push(fieldName, fieldValue);
      }
    break;
    case STATE_REQ_LINE:
      // Original HTTP parser ignored blank lines before request/status line,
      // so we do that here too ...
      if (line.length === 0)
        return true;
      var m = RE_REQUEST_LINE.exec(line);
      if (m === null) {
        this.execute = this._executeError;
        this._err = new Error('Malformed request line');
        return this._err;
      }
      // m[1]: HTTP method
      // m[2]: request target
      // m[3]: HTTP minor version
      var method = m[1];
      this.method = method;
      this.url = m[2];
      var minor = m[3];
      if (minor === undefined) {
        // HTTP/0.9 ugh...
        if (method !== 'GET') {
          this.execute = this._executeError;
          this._err = new Error('Malformed request line');
          return this._err;
        }
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
      if (m === null) {
        this.execute = this._executeError;
        this._err = new Error('Malformed status line');
        return this._err;
      }
      // m[1]: HTTP minor version
      // m[2]: HTTP status code
      // m[3]: Reason text
      this.httpMinor = parseInt(m[1], 10);
      this.statusCode = parseInt(m[2], 10);
      this.statusText = m[3] || '';
      this._state = STATE_HEADER;
    break;
    default:
      this.execute = this._executeError;
      this._err = new Error('Unexpected HTTP parser state: ' + this._state);
      return this._err;
  }
};
HTTPParser.prototype._headersEnd = function() {
  var flags = this._flags;
  var methodLower = this.method && this.method.toLowerCase();
  var upgrade = ((flags & FLAG_ANY_UPGRADE) === FLAG_ANY_UPGRADE ||
                  methodLower === 'connect');
  var keepalive = ((flags & FLAG_CONNECTION_CLOSE) === 0);
  var contentLen = this._contentLen;
  var ret;

  this._buf = '';
  this._seenCR = false;
  this._nbytes = 0;

  if ((this.httpMajor === 0 && this.httpMinor === 9) ||
      (this.httpMinor === 0 && (flags & FLAG_CONNECTION_KEEP_ALIVE) === 0)) {
    keepalive = false;
  }

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

  if ((flags & FLAG_TRAILING) > 0) {
    this.onComplete && this.onComplete();
    this.reinitialize(this.type);
    return;
  } else {
    var headers = this.headers;
    this.headers = [];
    if (this.onHeaders) {
      ret = this.onHeaders(this.httpMajor, this.httpMinor, headers, this.method,
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
    this.reinitialize(this.type);
  }
};
HTTPParser.prototype._executeHeader = function(data) {
  var offset = 0;
  var len = data.length;
  var idx;
  var seenCR = this._seenCR;
  var buf = this._buf;
  var ret;
  var bytesToAdd;
  var nhdrbytes = this._nhdrbytes;

  while (offset < len) {
    if (seenCR) {
      seenCR = false;
      if (data[offset] === LF) {
        // Our internal buffer contains a full line
        ++offset;
        ret = this._processHdrLine(buf);
        buf = '';
        if (typeof ret === 'object') {
          this._err = ret;
          return ret;
        } else if (ret === undefined) {
          var state = this._state;
          if (state !== STATE_HEADER) {
            // Begin of body or end of message
            if (state < STATE_COMPLETE && offset < len) {
              // Execute extra body bytes
              ret = this.execute(data.slice(offset));
              if (typeof ret !== 'number') {
                this._err = ret;
                return ret;
              }
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
          this.execute = this._executeError;
          this._err = new Error('Header size limit exceeded (' +
                                MAX_HEADER_BYTES + ')');
          return this._err;
        }
      }
    }
    var idx = data.indexOf(CRLF, offset);
    if (idx > -1) {
      // Our internal buffer contains a full line
      bytesToAdd = idx - offset;
      if (bytesToAdd > 0) {
        nhdrbytes += bytesToAdd;
        if (nhdrbytes > MAX_HEADER_BYTES) {
          this.execute = this._executeError;
          this._err = new Error('Header size limit exceeded (' +
                                MAX_HEADER_BYTES + ')');
          return this._err;
        }
        buf += data.toString('binary', offset, idx);
      }
      offset = idx + 2;
      ret = this._processHdrLine(buf);
      buf = '';
      if (typeof ret === 'object') {
        this._err = ret;
        return ret;
      } else if (ret === undefined) {
        var state = this._state;
        if (state !== STATE_HEADER) {
          // Begin of body or end of message
          if (state < STATE_COMPLETE && offset < len) {
            // Execute extra body bytes
            ret = this.execute(data.slice(offset));
            if (typeof ret !== 'number') {
              this._err = ret;
              return ret;
            }
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
        this.execute = this._executeError;
        this._err = new Error('Header size limit exceeded (' +
                              MAX_HEADER_BYTES + ')');
        return this._err;
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
    return;

  var offset = 0;
  var seenCR = this._seenCR;
  var buf = this._buf;
  var nbytes = this._nbytes;
  var idx;
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
              this._err = ret;
              return ret;
            } else if (ret === 0) {
              this._seenCR = false;
              this._buf = '';
              this._flags |= FLAG_TRAILING;
              this._state = STATE_HEADER;
              this.execute = this._executeHeader;
              ret = this.execute(data.slice(offset));
              if (typeof ret !== 'number') {
                this._err = ret;
                return ret;
              }
              return offset + ret;
            } else {
              nbytes = ret;
              this._state = STATE_BODY_CHUNKED_BYTES;
              continue;
            }
          } else {
            // False match
            buf += '\r';
            ++nbytes;

            if (nbytes > MAX_CHUNK_SIZE_LEN) {
              this.execute = this._executeError;
              this._err = new Error('Chunk size lint limit exceeded (' +
                                    MAX_CHUNK_SIZE_LEN + ')');
              return this._err;
            }
          }
        }
        var idx = data.indexOf(CRLF, offset);
        if (idx > -1) {
          // Our internal buffer contains a full line
          bytesToAdd = idx - offset;
          if (bytesToAdd > 0) {
            nbytes += bytesToAdd;
            if (nbytes > MAX_CHUNK_SIZE_LEN) {
              this.execute = this._executeError;
              this._err = new Error('Chunk size lint limit exceeded (' +
                                    MAX_CHUNK_SIZE_LEN + ')');
              return this._err;
            }
            buf += data.toString('ascii', offset, idx);
          }

          offset = idx + 2;
          ret = readChunkSize(buf);
          buf = '';

          if (typeof ret !== 'number') {
            this._err = ret;
            return ret;
          } else if (ret === 0) {
            this._seenCR = false;
            this._buf = '';
            this._flags |= FLAG_TRAILING;
            this._state = STATE_HEADER;
            this.execute = this._executeHeader;
            ret = this.execute(data.slice(offset));
            if (typeof ret !== 'number') {
              this._err = ret;
              return ret;
            }
            return offset + ret;
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

          bytesToAdd = end - offset;
          nbytes += bytesToAdd;

          if (nbytes > MAX_CHUNK_SIZE_LEN) {
            this.execute = this._executeError;
            this._err = new Error('Chunk size lint limit exceeded (' +
                                  MAX_CHUNK_SIZE_LEN + ')');
            return this._err;
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
        else {
          this.execute = this._executeError;
          this._err = new Error('Malformed chunk (missing CRLF)');
          return this._err;
        }
      break;
      default:
        this.execute = this._executeError;
        this._err = new Error('Unexpected parser state while reading chunks');
        return this._err;
    }
  }

  this._buf = buf;
  this._seenCR = seenCR;
  this._nbytes = nbytes;

  return len;
};
HTTPParser.prototype._executeBodyLiteral = function(data) {
  var len = data.length;
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
  this.onBody(data, 0, len);
  return len;
};
HTTPParser.prototype._executeBodyIgnore = function(data) {
  return 0;
};
HTTPParser.prototype._executeError = function(data) {
  return this._err;
};
HTTPParser.prototype.execute = HTTPParser.prototype._executeHeader;
HTTPParser.prototype._needsEOF = function() {
  if (this.type === HTTPParser.REQUEST)
    return false;

  // See RFC 2616 section 4.4
  var status = this.statusCode;
  var flags = this._flags;
  if ((status !== undefined &&
       (status === 204 ||                    // No Content
        status === 304 ||                    // Not Modified
        parseInt(status / 100, 1) === 1)) || // 1xx e.g. Continue
      flags & FLAG_SKIPBODY) {               // response to a HEAD request
    return false;
  }

  if ((flags & FLAG_CHUNKED) > 0 || this._contentLen != undefined)
    return false;

  return true;
}




HTTPParser.REQUEST = 0;
HTTPParser.RESPONSE = 1;

function readChunkSize(str) {
  var m = RE_CHUNK_LEN.exec(str);
  if (m === null || isNaN(m = parseInt(m[0], 16)) || m > MAX_CHUNK_SIZE)
    m = new Error('Invalid chunk size: ' + inspect(str));
  return m;
}


module.exports = HTTPParser;