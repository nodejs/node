/*
Misc differences with joyent/http-parser:
  * Folding whitespace behavior is conformant with RFC 7230:

      "A user agent that receives an obs-fold in a response message that is
       not within a message/http container MUST replace each received
       obs-fold with one or more SP octets prior to interpreting the field
       value."

    It should also be noted that RFC 7230 now deprecates line folding for HTTP
    parsing, FWIW. This parser replaces folds with a single SP octet.

  * Optional whitespace is removed before interpreting a header field value, as
    suggested by RFC 7230:

      "A field value might be preceded and/or followed by optional
       whitespace (OWS); a single SP preceding the field-value is preferred
       for consistent readability by humans.  The field value does not
       include any leading or trailing whitespace: OWS occurring before the
       first non-whitespace octet of the field value or after the last
       non-whitespace octet of the field value ought to be excluded by
       parsers when extracting the field value from a header field."

    joyent/http-parser keeps trailing whitespace. This parser keeps neither
    preceding nor trailing whitespace.

  * Does not allow spaces (which are invalid) in header field names.

  * Smaller maximum chunk/content length (2^53-1 vs 2^64-2). Obviously it's
    not *impossible* to handle a full 64-bit length, but it would mean adding
    some kind of "big integer" library for lengths > 2^53-1.

  * No special handling for `Proxy-Connection` header. The reason for this is
    that `Proxy-Connection` was an experimental header for HTTP 1.0 user agents
    that ended up being a bad idea because of the confusion it can bring. You
    can read a bit more about this in
    [RFC 7230 A.1.2](https://tools.ietf.org/html/rfc7230#page-79)
*/

var inspect = require('util').inspect;

var REQUEST = HTTPParser.REQUEST = 0;
var RESPONSE = HTTPParser.RESPONSE = 1;

var CR = 13;
var LF = 10;

var MAX_CHUNK_SIZE = Number.MAX_SAFE_INTEGER; // 9007199254740991

// RFC 7230 recommends HTTP implementations support at least 8000 bytes for
// the request line. We use 8190 by default, the same as Apache.
HTTPParser.MAX_REQ_LINE = 8190;
// RFC 7230 does not have any recommendations for minimum response line length
// support. Judging by the (current) longest standard status reason text, the
// typical response line will be 44 bytes or less (not including (CR)LF). Since
// the reason text field is free form though, we will roughly triple that
// amount for the default.
HTTPParser.MAX_RES_LINE = 128;

// This is the total limit for start line + all headers was copied from
// joyent/http-parser.
var MAX_HEADER_BYTES = 80 * 1024;

var RE_CONN_CLOSE = /(?:^|[\t ,]+)close(?:\r?$||[\t ,]+)/i;
var RE_CONN_KEEPALIVE = /(?:^|[\t ,]+)keep\-alive(?:\r?$|[\t ,]+)/i;
var RE_CONN_UPGRADE = /(?:^|[\t ,]+)upgrade(?:\r?$|[\t ,]+)/i;
var RE_TE_CHUNKED = /(?:^|[\t ,]+)chunked(?:\r?$|[\t ,]+)/i;
var CC_CONNECT = 'connect'.split('').map(getFirstCharCode);
var CC_CONNECTION = 'connection'.split('').map(getFirstCharCode);
var CC_TE = 'transfer-encoding'.split('').map(getFirstCharCode);
var CC_UPGRADE = 'upgrade'.split('').map(getFirstCharCode);
var CC_CONTLEN = 'content-length'.split('').map(getFirstCharCode);
var REQ_HTTP_VER_BYTES = ' HTTP/1.'.split('').map(getFirstCharCode);
var RES_HTTP_VER_BYTES = REQ_HTTP_VER_BYTES.slice(1);
REQ_HTTP_VER_BYTES.reverse();

var STATE_REQ_LINE = 0;
var STATE_STATUS_LINE = 1;
var STATE_HEADER = 2;
var STATE_BODY_LITERAL = 3;
var STATE_BODY_EOF = 4;
var STATE_BODY_CHUNKED_SIZE = 5;
var STATE_BODY_CHUNKED_SIZE_IGNORE = 6;
var STATE_BODY_CHUNKED_BYTES = 7;
var STATE_BODY_CHUNKED_BYTES_LF = 8;
var STATE_COMPLETE = 9;

var FLAG_CHUNKED = 1 << 0;
var FLAG_CONNECTION_KEEP_ALIVE = 1 << 1;
var FLAG_CONNECTION_CLOSE = 1 << 2;
var FLAG_CONNECTION_UPGRADE = 1 << 3;
var FLAG_TRAILING = 1 << 4;
var FLAG_UPGRADE = 1 << 5;
var FLAG_SKIPBODY = 1 << 6;
var FLAG_SHOULD_KEEP_ALIVE = 1 << 7;
var FLAG_CONNECT_METHOD = 1 << 8;
var FLAG_ANY_UPGRADE = FLAG_UPGRADE | FLAG_CONNECTION_UPGRADE;

function HTTPParser(type) {
  if (type === RESPONSE) {
    this.type = type;
    this._state = STATE_STATUS_LINE;
  }
  this.headers = [];
}
HTTPParser.prototype.type = REQUEST;
HTTPParser.prototype._state = STATE_REQ_LINE;
HTTPParser.prototype._err = null;
HTTPParser.prototype._flags = 0;
HTTPParser.prototype._contentLen = null;
HTTPParser.prototype._nbytes = null;
HTTPParser.prototype._nhdrbytes = 0;
HTTPParser.prototype._nhdrpairs = 0;
HTTPParser.prototype._buf = '';
HTTPParser.prototype.httpMajor = 1;
HTTPParser.prototype.httpMinor = null;
HTTPParser.prototype.maxHeaderPairs = 2000;
HTTPParser.prototype.method = null;
HTTPParser.prototype.url = null;
HTTPParser.prototype.statusCode = null;
HTTPParser.prototype.statusText = null;

HTTPParser.prototype.onHeaders = null;
HTTPParser.prototype.onBody = null;
HTTPParser.prototype.onComplete = null;

HTTPParser.prototype.onIncoming = null;
HTTPParser.prototype.incoming = null;
HTTPParser.prototype.socket = null;

HTTPParser.prototype.reinitialize = function(type) {
  this.execute = this._executeStartLine;
  this.type = type;
  if (type === REQUEST)
    this._state = STATE_REQ_LINE;
  else
    this._state = STATE_STATUS_LINE;

  this._err = null;
  this._flags = 0;
  this._contentLen = null;
  this._nbytes = null;
  this._nhdrbytes = 0;
  this._nhdrpairs = 0;
  this._buf = '';

  // Common properties
  this.headers = [];
  this.httpMajor = 1;
  this.httpMinor = null;
  this.maxHeaderPairs = 2000;

  // Request properties
  this.method = null;
  this.url = null;

  // Response properties
  this.statusCode = null;
  this.statusText = null;
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
HTTPParser.prototype._headersEnd = function() {
  var flags = this._flags;
  var type = this.type;
  var method = this.method;
  var upgrade = ((flags & FLAG_ANY_UPGRADE) === FLAG_ANY_UPGRADE ||
                 (flags & FLAG_CONNECT_METHOD) > 0);
  var contentLen = this._contentLen;
  var httpMajor = this.httpMajor;
  var httpMinor = this.httpMinor;
  var statusCode = this.statusCode;
  var keepalive = this._shouldKeepAlive(httpMajor, httpMinor, flags, type,
                                        statusCode);
  var ret;

  this._buf = '';
  this._nbytes = null;

  if ((flags & FLAG_CHUNKED) > 0) {
    this._state = STATE_BODY_CHUNKED_SIZE;
    this.execute = this._executeBodyChunked;
  } else if (contentLen !== null) {
    this._state = STATE_BODY_LITERAL;
    this.execute = this._executeBodyLiteral;
    if (keepalive)
      this._flags |= FLAG_SHOULD_KEEP_ALIVE;
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
    if (!keepalive)
      this._state = STATE_COMPLETE;
    else
      this.reinitialize(type);
    return;
  } else {
    this.headers = [];
    if (this.onHeaders) {
      if (headerslen > 0)
        headers[headerslen - 1] = trim(headers[headerslen - 1]);
      ret = this.onHeaders(httpMajor, httpMinor, headers, method,
                           this.url, statusCode, this.statusText, upgrade,
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
              contentLen === null &&
              !this._needsEOF(flags, type, statusCode)
             )) {
    this.onComplete && this.onComplete();
    this.reinitialize(type);
  }
};
HTTPParser.prototype._executeStartLine = function(data) {
  if (data.length === 0)
    return 0;
  var firstByte = data[0];
  if ((firstByte < 32 || firstByte >= 127) && firstByte !== CR &&
      firstByte !== LF) {
    return this._setError('Invalid byte in start line');
  }
  this.execute = this._executeHeader;
  return this.execute(data);
};
HTTPParser.prototype._executeHeader = function(data) {
  var len = data.length;

  if (len === 0)
    return 0;

  var offset = 0;
  var buf = this._buf;
  var nhdrbytes = this._nhdrbytes;
  var state = this._state;
  var headers = this.headers;
  var headerslen = headers.length;
  var maxHeaderPairs = this.maxHeaderPairs;
  var ret;

  while (offset < len) {
    ret = indexOfLF(data, len, offset);
    if (ret > -1) {
      // Our internal buffer contains a full line
      var bytesToAdd = ret - offset;
      if (bytesToAdd > 0) {
        nhdrbytes += bytesToAdd;
        if (state === STATE_REQ_LINE && nhdrbytes > HTTPParser.MAX_REQ_LINE)
          return this._setError('Request line limit exceeded');
        else if (state === STATE_STATUS_LINE &&
                 nhdrbytes > HTTPParser.MAX_RES_LINE) {
          return this._setError('Response line limit exceeded');
        } else if (nhdrbytes > MAX_HEADER_BYTES)
          return this._setError('Header limit exceeded');
        buf += data.toString('binary', offset, ret);
      }

      offset = ret + 1;
      var buflen = buf.length;

      switch (state) {
        case STATE_HEADER:
          if (buflen === 0 || buf.charCodeAt(0) === CR) {
            // We saw a double line ending
            this._headersEnd();
            state = this._state;
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
          var idx = -1;
          var fieldName;
          var fieldValue;
          var valueStart = -1;
          var validFieldName = true;
          for (var i = 0; i < buflen; ++i) {
            var ch = buf.charCodeAt(i);
            if (idx === -1) {
              if (ch === 58) { // ':'
                if (i === 0 || !validFieldName)
                  return this._setError('Malformed header line');
                idx = i;
              } else if (ch < 33 || ch > 126)
                validFieldName = false;
            } else if (ch !== 32 && ch !== 9) {
              valueStart = i;
              break;
            }
          }
          if (idx === -1) {
            var firstChr = buf.charCodeAt(0);
            if (firstChr !== 32 & firstChr !== 9)
              return this._setError('Malformed header line');
            // RFC 7230 compliant, but less backwards compatible:
            var extra = ltrim(buf);
            if (extra.length > 0) {
              if (headerslen === 0)
                return this._setError('Malformed header line');
              fieldName = headers[headerslen - 2];
              fieldValue = headers[headerslen - 1];
              if (fieldValue.length > 0) {
                if (fieldValue.charCodeAt(fieldValue.length - 1) === CR)
                  fieldValue = fieldValue.slice(0, -1);
                if (fieldValue.length > 0)
                  fieldValue += ' ' + extra;
                else
                  fieldValue = extra;
              } else
                fieldValue = extra;
              // Need to re-check value since matched values may now exist ...
              if (equalsLower(fieldName, CC_CONNECTION)) {
                if (fieldValue.search(RE_CONN_CLOSE) > -1)
                  this._flags |= FLAG_CONNECTION_CLOSE;
                if (fieldValue.search(RE_CONN_KEEPALIVE) > -1)
                  this._flags |= FLAG_CONNECTION_KEEP_ALIVE;
                if (fieldValue.search(RE_CONN_UPGRADE) > -1)
                  this._flags |= FLAG_CONNECTION_UPGRADE;
              } else if (equalsLower(fieldName, CC_TE)) {
                if (fieldValue.search(RE_TE_CHUNKED) > -1)
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
            fieldName = buf.slice(0, idx);
            fieldValue = valueStart === -1 ? '' : buf.slice(valueStart);
            // Ensures that trailing whitespace after the last folded line for
            // header values gets trimmed
            if (headerslen > 0)
              headers[headerslen - 1] = rtrim(headers[headerslen - 1]);
            if (equalsLower(fieldName, CC_CONNECTION)) {
              if (fieldValue.search(RE_CONN_CLOSE) > -1)
                this._flags |= FLAG_CONNECTION_CLOSE;
              if (fieldValue.search(RE_CONN_KEEPALIVE) > -1)
                this._flags |= FLAG_CONNECTION_KEEP_ALIVE;
              if (fieldValue.search(RE_CONN_UPGRADE) > -1)
                this._flags |= FLAG_CONNECTION_UPGRADE;
            } else if (equalsLower(fieldName, CC_TE)) {
              if (fieldValue.search(RE_TE_CHUNKED) > -1)
                this._flags |= FLAG_CHUNKED;
            } else if (equalsLower(fieldName, CC_UPGRADE)) {
              this._flags |= FLAG_UPGRADE;
            } else if (equalsLower(fieldName, CC_CONTLEN)) {
              var val = parseInt(fieldValue, 10);
              if (val !== val || val > MAX_CHUNK_SIZE)
                return this._setError('Bad Content-Length: ' + inspect(val));
              this._contentLen = val;
            }
            if (maxHeaderPairs <= 0 || ++this._nhdrpairs < maxHeaderPairs) {
              headers.push(fieldName, fieldValue);
              headerslen += 2;
            }
          }
          break;
        case STATE_REQ_LINE:
          // Original HTTP parser ignored blank lines before request/status
          // line, so we do that here too ...
          if (buflen === 0 || buf.charCodeAt(0) === CR)
            break;

          var firstSP;
          var urlStart;
          var urlEnd;
          var minor;
          var end = (buf.charCodeAt(buflen - 1) === CR ?
                     buflen - 3 : buflen - 2);
          // Start working backwards and both validate that the line ends in
          // ` HTTP/1.[01]` and find the end of the URL (in case there are
          // multiple spaces/tabs separating the URL and HTTP version
          var ch = buf.charCodeAt(end + 1);
          if (ch === 49)
            minor = 1;
          else if (ch === 48)
            minor = 0;
          else
            return this._setError('Malformed request line');
          var h = 0;
          while (end >= 0) {
            var ch = buf.charCodeAt(end);
            if (h < 8) {
              if (ch !== REQ_HTTP_VER_BYTES[h++])
                return this._setError('Malformed request line');
            } else if (ch >= 33 && ch !== 127) {
              urlEnd = end + 1;
              break;
            }
            --end;
          }
          if (urlEnd === undefined)
            return this._setError('Malformed request line');

          // Now start working forwards and both validate the HTTP method and
          // find the start of the URL (in case there are multiple spaces/tabs
          // separating the method and the URL
          var isConnect = false;
          var c = 0;
          for (var i = 0; i < urlEnd; ++i) {
            ch = buf.charCodeAt(i);
            if (firstSP !== undefined) {
              if (ch >= 33 && ch !== 127) {
                urlStart = i;
                break;
              }
            } else if (ch === 32) {
              firstSP = i;
              isConnect = (c === 7);
            } else if (ch < 33 || ch > 126)
              return this._setError('Malformed request line');
            else if (c >= 0) {
              if (c === 7)
                c = -1;
              else {
                ch |= 0x20;
                if (ch !== CC_CONNECT[c])
                  c = -1;
                else
                  ++c;
              }
            }
          }
          if (firstSP === undefined ||
              urlStart === undefined ||
              urlStart === urlEnd) {
            return this._setError('Malformed request line');
          }

          var url = buf.slice(urlStart, urlEnd);
          if (!validateUrl(url, isConnect))
            return this._setError('Malformed request line');

          if (isConnect)
            this._flags |= FLAG_CONNECT_METHOD;
          this.httpMinor = minor;
          this.method = buf.slice(0, firstSP);
          this.url = url;

          state = STATE_HEADER;
          break;
        case STATE_STATUS_LINE:
          // Original HTTP parser ignored blank lines before request/status
          // line, so we do that here too ...
          if (buflen === 0 || buf.charCodeAt(0) === CR)
            break;

          // Validate HTTP version
          for (var h = 0; i < 7; ++h) {
            if (buf.charCodeAt(i) !== RES_HTTP_VER_BYTES[h])
              return this._setError('Malformed status line');
          }
          var minor;
          var status = 0;
          if (buf.charCodeAt(7) === 49)
            minor = 1;
          else if (buf.charCodeAt(7) === 48)
            minor = 0;
          else
            return this._setError('Malformed status line');
          if (buf.charCodeAt(8) !== 32)
            return this._setError('Malformed status line');

          // Validate status code
          for (var i = 9; i < 12; ++i) {
            var ch = buf.charCodeAt(i);
            if (ch < 48 || ch > 57)
              return this._setError('Malformed status line');
            status *= 10;
            status += (ch - 48);
          }

          if (buf.charCodeAt(buflen - 1))
            --buflen;
          this.httpMinor = minor;
          this.statusCode = status;
          this.statusText = (buflen > 13 ? buf.slice(13, buflen) : '');
          state = STATE_HEADER;
          break;
        default:
          return this._setError('Unexpected HTTP parser state: ' + state);
      }

      buf = '';
    } else {
      nhdrbytes += len - offset;
      if (state === STATE_REQ_LINE && nhdrbytes > HTTPParser.MAX_REQ_LINE)
        return this._setError('Request line limit exceeded');
      else if (state === STATE_STATUS_LINE &&
               nhdrbytes > HTTPParser.MAX_RES_LINE) {
        return this._setError('Response line limit exceeded');
      } else if (nhdrbytes > MAX_HEADER_BYTES)
        return this._setError('Header limit exceeded');
      buf += data.toString('binary', offset);
      break;
    }
  }

  this._state = state;
  this._buf = buf;
  this._nhdrbytes = nhdrbytes;

  return len;
};
HTTPParser.prototype._executeBodyChunked = function(data) {
  var len = data.length;

  if (len === 0)
    return 0;

  var offset = 0;
  var nbytes = this._nbytes;
  var state = this._state;
  var dec;

  while (offset < len) {
    switch (state) {
      case STATE_BODY_CHUNKED_SIZE:
        while (offset < len) {
          var ch = data[offset];
          dec = hexValue(ch);
          if (dec === undefined) {
            if (nbytes === null)
              return this._setError('Invalid chunk size');
            state = STATE_BODY_CHUNKED_SIZE_IGNORE;
            break;
          } else if (nbytes === null)
            nbytes = dec;
          else {
            nbytes *= 16;
            nbytes += dec;
          }
          if (nbytes > MAX_CHUNK_SIZE)
            return this._setError('Chunk size limit exceeded');
          ++offset;
        }
        break;
      case STATE_BODY_CHUNKED_BYTES:
        var dataleft = len - offset;
        if (dataleft >= nbytes) {
          this.onBody(data, offset, nbytes);
          offset += nbytes;
          nbytes = 0;
          state = STATE_BODY_CHUNKED_BYTES_LF;
        } else {
          nbytes -= dataleft;
          this.onBody(data, offset, dataleft);
          offset = len;
        }
        break;
      case STATE_BODY_CHUNKED_BYTES_LF:
        // We reach this state after all chunk bytes have been read and we are
        // looking for the (CR)LF following the bytes
        while (offset < len) {
          var curByte = data[offset++];
          if (nbytes === 0) {
            if (curByte === LF) {
              state = STATE_BODY_CHUNKED_SIZE;
              nbytes = null;
              break;
            } else if (curByte === CR)
              ++nbytes;
          } else if (nbytes === 1 && curByte === LF) {
            state = STATE_BODY_CHUNKED_SIZE;
            nbytes = null;
            break;
          } else
            return this._setError('Malformed chunk (malformed line ending)');
        }
        break;
      case STATE_BODY_CHUNKED_SIZE_IGNORE:
        // We only reach this state once we receive a non-hex character on the
        // chunk size line
        while (offset < len) {
          if (data[offset++] === LF) {
            if (nbytes === 0) {
              this._flags |= FLAG_TRAILING;
              this._state = STATE_HEADER;
              this._nbytes = null;
              this.execute = this._executeHeader;
              if (offset < len) {
                var ret = this.execute(data.slice(offset));
                if (typeof ret !== 'number')
                  return ret;
                return offset + ret;
              }
              return offset;
            } else {
              state = STATE_BODY_CHUNKED_BYTES;
              break;
            }
          }
        }
        break;
      default:
        return this._setError('Unexpected parser state while reading chunks');
    }
  }

  this._state = state;
  this._nbytes = nbytes;

  return len;
};
HTTPParser.prototype._executeBodyLiteral = function(data) {
  var len = data.length;

  if (len === 0)
    return 0;

  var nbytes = this._contentLen;
  if (len >= nbytes) {
    this.onBody(data, 0, nbytes);
    this.onComplete && this.onComplete();
    if ((this._flags & FLAG_SHOULD_KEEP_ALIVE) > 0) {
      this.reinitialize(this.type);
      if (len > nbytes) {
        var ret = this.execute(data.slice(nbytes));
        if (typeof ret !== 'number')
          return ret;
        return nbytes + ret;
      }
    } else
      this._state = STATE_COMPLETE;
  } else {
    this._contentLen -= len;
    this.onBody(data, 0, len);
  }
  return len;
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
HTTPParser.prototype._shouldKeepAlive = function(httpMajor, httpMinor, flags,
                                                 type, status) {
  if (httpMajor > 0 && httpMinor > 0) {
    if ((flags & FLAG_CONNECTION_CLOSE) > 0)
      return false;
  } else {
    if ((flags & FLAG_CONNECTION_KEEP_ALIVE) === 0)
      return false;
  }
  return !this._needsEOF(flags, type, status);
};
HTTPParser.prototype._needsEOF = function(flags, type, status) {
  if (type === REQUEST)
    return false;

  // See RFC 2616 section 4.4
  if (status === 204 ||                    // No Content
      status === 304 ||                    // Not Modified
      (status >= 100 && status < 200) ||   // 1xx e.g. Continue
      (flags & FLAG_SKIPBODY) > 0) {       // response to a HEAD request
    return false;
  }

  if ((flags & FLAG_CHUNKED) > 0 || this._contentLen !== null)
    return false;

  return true;
};




module.exports = HTTPParser;

function indexOfLF(buf, buflen, offset) {
  while (offset < buflen) {
    if (buf[offset] === LF)
      return offset;
    ++offset;
  }
  return -1;
}

function hexValue(ch) {
  if (ch > 47 && ch < 58)
    return ch - 48;
  ch |= 0x20;
  if (ch > 96 && ch < 103)
    return 10 + (ch - 97);
}

function ltrim(value) {
  var length = value.length, start;
  for (start = 0;
       start < length &&
       (value.charCodeAt(start) === 32 || value.charCodeAt(start) === 9 ||
        value.charCodeAt(start) === CR);
       ++start);
  return (start > 0 ? value.slice(start) : value);
}

function rtrim(value) {
  var length = value.length, end;
  for (end = length;
       end > 0 &&
       (value.charCodeAt(end - 1) === 32 || value.charCodeAt(end - 1) === 9 ||
        value.charCodeAt(end - 1) === CR);
       --end);
  return (end < length ? value.slice(0, end) : value);
}

function trim(value) {
  var length = value.length, start, end;
  for (start = 0;
       start < length &&
       (value.charCodeAt(start) === 32 || value.charCodeAt(start) === 9 ||
        value.charCodeAt(start) === CR);
       ++start);
  for (end = length;
       end > start &&
       (value.charCodeAt(end - 1) === 32 || value.charCodeAt(end - 1) === 9 ||
        value.charCodeAt(end - 1) === CR);
       --end);
  return (start > 0 || end < length ? value.slice(start, end) : value);
}

function equalsLower(input, ref) {
  var inlen = input.length;
  if (inlen !== ref.length)
    return false;
  for (var i = 0; i < inlen; ++i) {
    if ((input.charCodeAt(i) | 0x20) !== ref[i])
      return false;
  }
  return true;
}

function validateUrl(url, isConnect) {
  var state = (isConnect ? 5 : 0);
  var ch;
  for (var i = 0; i < url.length; ++i) {
    ch = url.charCodeAt(i);
    switch (state) {
      case 0:
        // Proxied requests are followed by scheme of an absolute URI (alpha).
        // All methods except CONNECT are followed by '/' or '*'.
        if (ch === 47 || // '/'
            ch === 42) { // '*'
          state = 6;
          continue;
        } else if ((ch |= 0x20) > 96 && ch < 123) { // A-Za-z
          state = 1;
          continue;
        }
        break;
      case 1:
        if (ch === 58) { // ':'
          state = 2;
          continue;
        } else if ((ch |= 0x20) > 96 && ch < 123) // A-Za-z
          continue;
        break;
      case 2:
        if (ch === 47) { // '/'
          state = 3;
          continue;
        }
        break;
      case 3:
        if (ch === 47) { // '/'
          state = 5;
          continue;
        }
        break;
      case 4:
        if (ch === 64) // '@'
          return false;
      // FALLTHROUGH
      case 5:
        if (ch === 47) { // '/'
          state = 6;
          continue;
        } else if (ch === 63) { // '?'
          state = 7;
          continue;
        } else if (ch === 64) { // '@'
          state = 4;
          continue;
        } else if ((ch > 35 && ch < 58) || // 0-9
                    ch === 33 ||  // '!'
                    ch === 58 ||  // ':'
                    ch === 59 ||  // ';'
                    ch === 61 ||  // '='
                    ch === 91 ||  // '['
                    ch === 93 ||  // ']'
                    ch === 95 ||  // '_'
                    ch === 126 || // '~'
                    ((ch |= 0x20) > 96 && ch < 123)) { // A-Za-z
          continue;
        }
        break;
      case 6:
        if (ch === 63) { // '?'
          state = 7;
          continue;
        } else if (ch === 35) { // '#'
          state = 8;
          continue;
        // http://jsperf.com/check-url-character
        } else if (!(ch < 33 || ch === 127)) // Normal URL characters
          continue;
        break;
      case 7:
        if (ch === 63) { // '?'
          // Allow extra '?' in query string
          continue;
        } else if (ch === 35) { // '#'
          state = 8;
          continue;
        } else if (!(ch < 33 || ch === 127)) // Normal URL characters
          continue;
        break;
      case 8:
        if (ch === 63) { // '?'
          state = 9;
          continue;
        } else if (ch === 35) // '#'
          continue;
        else if (!(ch < 33 || ch === 127)) { // Normal URL characters
          state = 9;
          continue;
        }
        break;
      case 9:
        if (ch === 63 || // '?'
            ch === 35 || // '#'
            !(ch < 33 || ch === 127)) { // Normal URL characters
          continue;
        }
        break;
    }
    return false;
  }
  return true;
}

function getFirstCharCode(str) {
  return str.charCodeAt(0);
}
