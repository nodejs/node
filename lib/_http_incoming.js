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

const Stream = require('stream');

function readStart(socket) {
  if (socket && !socket._paused && socket.readable)
    socket.resume();
}

function readStop(socket) {
  if (socket)
    socket.pause();
}

/* Abstract base class for ServerRequest and ClientResponse. */
function IncomingMessage(socket) {
  Stream.Readable.call(this);

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

  this.aborted = false;

  this.upgrade = null;

  // request (server) only
  this.url = '';
  this.method = null;

  // response (client) only
  this.statusCode = null;
  this.statusMessage = null;
  this.client = socket;

  this._consuming = false;
  // Flag for when we decide that this message cannot possibly be
  // read by the user, so there's no point continuing to handle it.
  this._dumped = false;
}
Object.setPrototypeOf(IncomingMessage.prototype, Stream.Readable.prototype);
Object.setPrototypeOf(IncomingMessage, Stream.Readable);

IncomingMessage.prototype.setTimeout = function setTimeout(msecs, callback) {
  if (callback)
    this.on('timeout', callback);
  this.socket.setTimeout(msecs);
  return this;
};


IncomingMessage.prototype._read = function _read(n) {
  if (!this._consuming) {
    this._readableState.readingMore = false;
    this._consuming = true;
  }

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
      this._addHeaderLine(headers[i], headers[i + 1], dest);
    }
  }
}


// This function is used to help avoid the lowercasing of a field name if it
// matches a 'traditional cased' version of a field name. It then returns the
// lowercased name to both avoid calling toLowerCase() a second time and to
// indicate whether the field was a 'no duplicates' field. If a field is not a
// 'no duplicates' field, a `0` byte is prepended as a flag. The one exception
// to this is the Set-Cookie header which is indicated by a `1` byte flag, since
// it is an 'array' field and thus is treated differently in _addHeaderLines().
// TODO: perhaps http_parser could be returning both raw and lowercased versions
// of known header names to avoid us having to call toLowerCase() for those
// headers.
function matchKnownFields(field, lowercased) {
  switch (field.length) {
    case 3:
      if (field === 'Age' || field === 'age') return 'age';
      break;
    case 4:
      if (field === 'Host' || field === 'host') return 'host';
      if (field === 'From' || field === 'from') return 'from';
      if (field === 'ETag' || field === 'etag') return 'etag';
      if (field === 'Date' || field === 'date') return '\u0000date';
      if (field === 'Vary' || field === 'vary') return '\u0000vary';
      break;
    case 6:
      if (field === 'Server' || field === 'server') return 'server';
      if (field === 'Cookie' || field === 'cookie') return '\u0002cookie';
      if (field === 'Origin' || field === 'origin') return '\u0000origin';
      if (field === 'Expect' || field === 'expect') return '\u0000expect';
      if (field === 'Accept' || field === 'accept') return '\u0000accept';
      break;
    case 7:
      if (field === 'Referer' || field === 'referer') return 'referer';
      if (field === 'Expires' || field === 'expires') return 'expires';
      if (field === 'Upgrade' || field === 'upgrade') return '\u0000upgrade';
      break;
    case 8:
      if (field === 'Location' || field === 'location')
        return 'location';
      if (field === 'If-Match' || field === 'if-match')
        return '\u0000if-match';
      break;
    case 10:
      if (field === 'User-Agent' || field === 'user-agent')
        return 'user-agent';
      if (field === 'Set-Cookie' || field === 'set-cookie')
        return '\u0001';
      if (field === 'Connection' || field === 'connection')
        return '\u0000connection';
      break;
    case 11:
      if (field === 'Retry-After' || field === 'retry-after')
        return 'retry-after';
      break;
    case 12:
      if (field === 'Content-Type' || field === 'content-type')
        return 'content-type';
      if (field === 'Max-Forwards' || field === 'max-forwards')
        return 'max-forwards';
      break;
    case 13:
      if (field === 'Authorization' || field === 'authorization')
        return 'authorization';
      if (field === 'Last-Modified' || field === 'last-modified')
        return 'last-modified';
      if (field === 'Cache-Control' || field === 'cache-control')
        return '\u0000cache-control';
      if (field === 'If-None-Match' || field === 'if-none-match')
        return '\u0000if-none-match';
      break;
    case 14:
      if (field === 'Content-Length' || field === 'content-length')
        return 'content-length';
      break;
    case 15:
      if (field === 'Accept-Encoding' || field === 'accept-encoding')
        return '\u0000accept-encoding';
      if (field === 'Accept-Language' || field === 'accept-language')
        return '\u0000accept-language';
      if (field === 'X-Forwarded-For' || field === 'x-forwarded-for')
        return '\u0000x-forwarded-for';
      break;
    case 16:
      if (field === 'Content-Encoding' || field === 'content-encoding')
        return '\u0000content-encoding';
      if (field === 'X-Forwarded-Host' || field === 'x-forwarded-host')
        return '\u0000x-forwarded-host';
      break;
    case 17:
      if (field === 'If-Modified-Since' || field === 'if-modified-since')
        return 'if-modified-since';
      if (field === 'Transfer-Encoding' || field === 'transfer-encoding')
        return '\u0000transfer-encoding';
      if (field === 'X-Forwarded-Proto' || field === 'x-forwarded-proto')
        return '\u0000x-forwarded-proto';
      break;
    case 19:
      if (field === 'Proxy-Authorization' || field === 'proxy-authorization')
        return 'proxy-authorization';
      if (field === 'If-Unmodified-Since' || field === 'if-unmodified-since')
        return 'if-unmodified-since';
      break;
  }
  if (lowercased) {
    return '\u0000' + field;
  } else {
    return matchKnownFields(field.toLowerCase(), true);
  }
}
// Add the given (field, value) pair to the message
//
// Per RFC2616, section 4.2 it is acceptable to join multiple instances of the
// same header with a ', ' if the header in question supports specification of
// multiple values this way. The one exception to this is the Cookie header,
// which has multiple values joined with a '; ' instead. If a header's values
// cannot be joined in either of these ways, we declare the first instance the
// winner and drop the second. Extended header fields (those beginning with
// 'x-') are always joined.
IncomingMessage.prototype._addHeaderLine = _addHeaderLine;
function _addHeaderLine(field, value, dest) {
  field = matchKnownFields(field);
  var flag = field.charCodeAt(0);
  if (flag === 0 || flag === 2) {
    field = field.slice(1);
    // Make a delimited list
    if (typeof dest[field] === 'string') {
      dest[field] += (flag === 0 ? ', ' : '; ') + value;
    } else {
      dest[field] = value;
    }
  } else if (flag === 1) {
    // Array header -- only Set-Cookie at the moment
    if (dest['set-cookie'] !== undefined) {
      dest['set-cookie'].push(value);
    } else {
      dest['set-cookie'] = [value];
    }
  } else if (dest[field] === undefined) {
    // Drop duplicates
    dest[field] = value;
  }
}


// Call this instead of resume() if we want to just
// dump all the data to /dev/null
IncomingMessage.prototype._dump = function _dump() {
  if (!this._dumped) {
    this._dumped = true;
    // If there is buffered data, it may trigger 'data' events.
    // Remove 'data' event listeners explicitly.
    this.removeAllListeners('data');
    this.resume();
  }
};

module.exports = {
  IncomingMessage,
  readStart,
  readStop
};
