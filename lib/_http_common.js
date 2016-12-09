'use strict';

const binding = process.binding('http_parser');
const methods = binding.methods;
const HTTPParser = binding.HTTPParser;

const FreeList = require('internal/freelist').FreeList;
const incoming = require('_http_incoming');
const IncomingMessage = incoming.IncomingMessage;
const readStart = incoming.readStart;
const readStop = incoming.readStop;

const debug = require('util').debuglog('http');
exports.debug = debug;

exports.CRLF = '\r\n';
exports.chunkExpression = /chunk/i;
exports.continueExpression = /100-continue/i;
exports.methods = methods;

const kOnHeaders = HTTPParser.kOnHeaders | 0;
const kOnHeadersComplete = HTTPParser.kOnHeadersComplete | 0;
const kOnBody = HTTPParser.kOnBody | 0;
const kOnMessageComplete = HTTPParser.kOnMessageComplete | 0;
const kOnExecute = HTTPParser.kOnExecute | 0;

// Only called in the slow case where slow means
// that the request headers were either fragmented
// across multiple TCP packets or too large to be
// processed in a single run. This method is also
// called to process trailing HTTP headers.
function parserOnHeaders(headers, url) {
  // Once we exceeded headers limit - stop collecting them
  if (this.maxHeaderPairs <= 0 ||
      this._headers.length < this.maxHeaderPairs) {
    this._headers = this._headers.concat(headers);
  }
  this._url += url;
}

// `headers` and `url` are set only if .onHeaders() has not been called for
// this request.
// `url` is not set for response parsers but that's not applicable here since
// all our parsers are request parsers.
function parserOnHeadersComplete(versionMajor, versionMinor, headers, method,
                                 url, statusCode, statusMessage, upgrade,
                                 shouldKeepAlive) {
  var parser = this;

  if (!headers) {
    headers = parser._headers;
    parser._headers = [];
  }

  if (!url) {
    url = parser._url;
    parser._url = '';
  }

  parser.incoming = new IncomingMessage(parser.socket);
  parser.incoming.httpVersionMajor = versionMajor;
  parser.incoming.httpVersionMinor = versionMinor;
  parser.incoming.httpVersion = versionMajor + '.' + versionMinor;
  parser.incoming.url = url;

  var n = headers.length;

  // If parser.maxHeaderPairs <= 0 assume that there's no limit.
  if (parser.maxHeaderPairs > 0)
    n = Math.min(n, parser.maxHeaderPairs);

  parser.incoming._addHeaderLines(headers, n);

  if (typeof method === 'number') {
    // server only
    parser.incoming.method = methods[method];
  } else {
    // client only
    parser.incoming.statusCode = statusCode;
    parser.incoming.statusMessage = statusMessage;
  }

  if (upgrade && parser.outgoing !== null && !parser.outgoing.upgrading) {
    // The client made non-upgrade request, and server is just advertising
    // supported protocols.
    //
    // See RFC7230 Section 6.7
    upgrade = false;
  }

  parser.incoming.upgrade = upgrade;

  var skipBody = 0; // response to HEAD or CONNECT

  if (!upgrade) {
    // For upgraded connections and CONNECT method request, we'll emit this
    // after parser.execute so that we can capture the first part of the new
    // protocol.
    skipBody = parser.onIncoming(parser.incoming, shouldKeepAlive);
  }

  if (typeof skipBody !== 'number')
    return skipBody ? 1 : 0;
  else
    return skipBody;
}

// XXX This is a mess.
// TODO: http.Parser should be a Writable emits request/response events.
function parserOnBody(b, start, len) {
  var parser = this;
  var stream = parser.incoming;

  // if the stream has already been removed, then drop it.
  if (!stream)
    return;

  var socket = stream.socket;

  // pretend this was the result of a stream._read call.
  if (len > 0 && !stream._dumped) {
    var slice = b.slice(start, start + len);
    var ret = stream.push(slice);
    if (!ret)
      readStop(socket);
  }
}

function parserOnMessageComplete() {
  var parser = this;
  var stream = parser.incoming;

  if (stream) {
    stream.complete = true;
    // Emit any trailing headers.
    var headers = parser._headers;
    if (headers) {
      parser.incoming._addHeaderLines(headers, headers.length);
      parser._headers = [];
      parser._url = '';
    }

    // For emit end event
    stream.push(null);
  }

  // force to read the next incoming message
  readStart(parser.socket);
}


var parsers = new FreeList('parsers', 1000, function() {
  var parser = new HTTPParser(HTTPParser.REQUEST);

  parser._headers = [];
  parser._url = '';
  parser._consumed = false;

  parser.socket = null;
  parser.incoming = null;
  parser.outgoing = null;

  // Only called in the slow case where slow means
  // that the request headers were either fragmented
  // across multiple TCP packets or too large to be
  // processed in a single run. This method is also
  // called to process trailing HTTP headers.
  parser[kOnHeaders] = parserOnHeaders;
  parser[kOnHeadersComplete] = parserOnHeadersComplete;
  parser[kOnBody] = parserOnBody;
  parser[kOnMessageComplete] = parserOnMessageComplete;
  parser[kOnExecute] = null;

  return parser;
});
exports.parsers = parsers;


// Free the parser and also break any links that it
// might have to any other things.
// TODO: All parser data should be attached to a
// single object, so that it can be easily cleaned
// up by doing `parser.data = {}`, which should
// be done in FreeList.free.  `parsers.free(parser)`
// should be all that is needed.
function freeParser(parser, req, socket) {
  if (parser) {
    parser._headers = [];
    parser.onIncoming = null;
    if (parser._consumed)
      parser.unconsume();
    parser._consumed = false;
    if (parser.socket)
      parser.socket.parser = null;
    parser.socket = null;
    parser.incoming = null;
    parser.outgoing = null;
    parser[kOnExecute] = null;
    if (parsers.free(parser) === false)
      parser.close();
    parser = null;
  }
  if (req) {
    req.parser = null;
  }
  if (socket) {
    socket.parser = null;
  }
}
exports.freeParser = freeParser;


function ondrain() {
  if (this._httpMessage) this._httpMessage.emit('drain');
}


function httpSocketSetup(socket) {
  socket.removeListener('drain', ondrain);
  socket.on('drain', ondrain);
}
exports.httpSocketSetup = httpSocketSetup;

/**
 * Verifies that the given val is a valid HTTP token
 * per the rules defined in RFC 7230
 * See https://tools.ietf.org/html/rfc7230#section-3.2.6
 *
 * Allowed characters in an HTTP token:
 * ^_`a-z  94-122
 * A-Z     65-90
 * -       45
 * 0-9     48-57
 * !       33
 * #$%&'   35-39
 * *+      42-43
 * .       46
 * |       124
 * ~       126
 *
 * This implementation of checkIsHttpToken() loops over the string instead of
 * using a regular expression since the former is up to 180% faster with v8 4.9
 * depending on the string length (the shorter the string, the larger the
 * performance difference)
 *
 * Additionally, checkIsHttpToken() is currently designed to be inlinable by v8,
 * so take care when making changes to the implementation so that the source
 * code size does not exceed v8's default max_inlined_source_size setting.
 **/
function isValidTokenChar(ch) {
  if (ch >= 94 && ch <= 122)
    return true;
  if (ch >= 65 && ch <= 90)
    return true;
  if (ch === 45)
    return true;
  if (ch >= 48 && ch <= 57)
    return true;
  if (ch === 34 || ch === 40 || ch === 41 || ch === 44)
    return false;
  if (ch >= 33 && ch <= 46)
    return true;
  if (ch === 124 || ch === 126)
    return true;
  return false;
}
function checkIsHttpToken(val) {
  if (typeof val !== 'string' || val.length === 0)
    return false;
  if (!isValidTokenChar(val.charCodeAt(0)))
    return false;
  const len = val.length;
  if (len > 1) {
    if (!isValidTokenChar(val.charCodeAt(1)))
      return false;
    if (len > 2) {
      if (!isValidTokenChar(val.charCodeAt(2)))
        return false;
      if (len > 3) {
        if (!isValidTokenChar(val.charCodeAt(3)))
          return false;
        for (var i = 4; i < len; i++) {
          if (!isValidTokenChar(val.charCodeAt(i)))
            return false;
        }
      }
    }
  }
  return true;
}
exports._checkIsHttpToken = checkIsHttpToken;

/**
 * True if val contains an invalid field-vchar
 *  field-value    = *( field-content / obs-fold )
 *  field-content  = field-vchar [ 1*( SP / HTAB ) field-vchar ]
 *  field-vchar    = VCHAR / obs-text
 *
 * checkInvalidHeaderChar() is currently designed to be inlinable by v8,
 * so take care when making changes to the implementation so that the source
 * code size does not exceed v8's default max_inlined_source_size setting.
 **/
function checkInvalidHeaderChar(val) {
  val += '';
  if (val.length < 1)
    return false;
  var c = val.charCodeAt(0);
  if ((c <= 31 && c !== 9) || c > 255 || c === 127)
    return true;
  if (val.length < 2)
    return false;
  c = val.charCodeAt(1);
  if ((c <= 31 && c !== 9) || c > 255 || c === 127)
    return true;
  if (val.length < 3)
    return false;
  c = val.charCodeAt(2);
  if ((c <= 31 && c !== 9) || c > 255 || c === 127)
    return true;
  for (var i = 3; i < val.length; ++i) {
    c = val.charCodeAt(i);
    if ((c <= 31 && c !== 9) || c > 255 || c === 127)
      return true;
  }
  return false;
}
exports._checkInvalidHeaderChar = checkInvalidHeaderChar;
