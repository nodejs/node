'use strict';

const binding = process.binding('http_parser');
const knownHttpHeaders = binding.knownHttpHeaders;
const util = require('util');
const Stream = require('stream');
const lowerCaseHttpHeaderLookup = (function() {
  var code = 'return Object.create(null, {';
  for (var i = 0; i < knownHttpHeaders.length; ++i) {
    const origName = knownHttpHeaders[i][1];
    const lowName = knownHttpHeaders[i][2];
    code += `${JSON.stringify(origName)}: {`;
    code += `value: ${JSON.stringify(lowName)}, enumerable: true`;
    code += '},';
  }
  code += '})';
  return new Function(code)();
})();


function readStart(socket) {
  if (socket && !socket._paused && socket.readable)
    socket.resume();
}
exports.readStart = readStart;

function readStop(socket) {
  if (socket)
    socket.pause();
}
exports.readStop = readStop;


/* Abstract base class for ServerRequest and ClientResponse. */
function IncomingMessage(socket) {
  Stream.Readable.call(this);

  // Set this to `true` so that stream.Readable won't attempt to read more
  // data on `IncomingMessage#push` (see `maybeReadMore` in
  // `_stream_readable.js`). This is important for proper tracking of
  // `IncomingMessage#_consuming` which is used to dump requests that users
  // haven't attempted to read.
  this._readableState.readingMore = true;

  this.socket = socket;
  this.connection = socket;

  this.httpVersionMajor = null;
  this.httpVersionMinor = null;
  this.httpVersion = null;
  this.complete = false;
  this.headers = {};
  this.rawHeaders = [];
  this.trailers = {};
  this.rawTrailers = [];

  this.readable = true;

  this.upgrade = null;

  // request (server) only
  this.url = '';
  this.method = null;

  // response (client) only
  this.statusCode = null;
  this.statusMessage = null;
  this.client = socket;

  // flag for backwards compatibility grossness.
  this._consuming = false;

  // flag for when we decide that this message cannot possibly be
  // read by the user, so there's no point continuing to handle it.
  this._dumped = false;
}
util.inherits(IncomingMessage, Stream.Readable);


exports.IncomingMessage = IncomingMessage;


IncomingMessage.prototype.setTimeout = function setTimeout(msecs, callback) {
  if (callback)
    this.on('timeout', callback);
  this.socket.setTimeout(msecs);
  return this;
};


IncomingMessage.prototype.read = function read(n) {
  if (!this._consuming)
    this._readableState.readingMore = false;
  this._consuming = true;
  this.read = Stream.Readable.prototype.read;
  return this.read(n);
};


IncomingMessage.prototype._read = function _read(n) {
  // We actually do almost nothing here, because the parserOnBody
  // function fills up our internal buffer directly.  However, we
  // do need to unpause the underlying socket so that it flows.
  if (this.socket.readable)
    readStart(this.socket);
};


// It's possible that the socket will be destroyed, and removed from
// any messages, before ever calling this.  In that case, just skip
// it, since something else is destroying this connection anyway.
IncomingMessage.prototype.destroy = function destroy(error) {
  if (this.socket)
    this.socket.destroy(error);
};


IncomingMessage.prototype._addHeaderLines = _addHeaderLines;
function _addHeaderLines(headers, n) {
  if (headers && headers.length) {
    var dest;
    if (this.complete) {
      this.rawTrailers = headers;
      dest = this.trailers;
    } else {
      this.rawHeaders = headers;
      dest = this.headers;
    }

    for (var i = 0; i < n; i += 2) {
      if (typeof headers[i] === 'number') {
        var headerEntry = knownHttpHeaders[headers[i]];
        headers[i] = headerEntry[0];
        _saveHeaderLine(headerEntry[1], headers[i + 1], headerEntry[2], dest);
      } else {
        _addHeaderLine(headers[i], headers[i + 1], dest);
      }
    }
  }
}

// Save the given (field, value) pair to the message.
// Note: This is a static function and not on IncomingMessage unlike
// _addHeaderLine. This method is created in addition to _addHeaderLine so:
// 1. Backward compatibility is maintained for user code that use _addHeaderLine
// 2. User code can't call _saveHeaderLine
//
// Per RFC2616, section 4.2 it is acceptable to join multiple instances of the
// same header with a ', ' if the header in question supports specification of
// multiple values this way. If not, we declare the first instance the winner
// and drop the second. Extended header fields (those beginning with 'x-') are
// always joined.
function _saveHeaderLine(lowercaseField, value, flag, dest) {
  if (flag === 0) {
    // Make comma-separated list
    if (typeof dest[lowercaseField] === 'string') {
      dest[lowercaseField] += ', ' + value;
    } else {
      dest[lowercaseField] = value;
    }
  } else if (flag === 1) {
    // Array header -- only Set-Cookie at the moment
    if (dest['set-cookie'] !== undefined) {
      dest['set-cookie'].push(value);
    } else {
      dest['set-cookie'] = [value];
    }
  } else {
    // Drop duplicates
    if (dest[lowercaseField] === undefined)
      dest[lowercaseField] = value;
  }
}

IncomingMessage.prototype._addHeaderLine = _addHeaderLine;
function _addHeaderLine(field, value, dest) {
  // Check if field is already lower-case and one of the known header.
  var flag = lowerCaseHttpHeaderLookup[field];
  // If not, convert to lower and try one more time.
  if (flag === undefined) {
    field = field.toLowerCase();
    flag = lowerCaseHttpHeaderLookup[field] || 0;
  }
  _saveHeaderLine(field, value, flag, dest);
}


// Call this instead of resume() if we want to just
// dump all the data to /dev/null
IncomingMessage.prototype._dump = function _dump() {
  if (!this._dumped) {
    this._dumped = true;
    this.resume();
  }
};
