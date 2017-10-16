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

const util = require('util');
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

// 'array' header list is taken from:
// https://mxr.mozilla.org/mozilla/source/netwerk/protocol/http/src/nsHttpHeaderArray.cpp
function matchKnownFields(field) {
  var low = false;
  while (true) {
    switch (field) {
      case 'Content-Type':
      case 'content-type':
        return 'content-type';
      case 'Content-Length':
      case 'content-length':
        return 'content-length';
      case 'User-Agent':
      case 'user-agent':
        return 'user-agent';
      case 'Referer':
      case 'referer':
        return 'referer';
      case 'Host':
      case 'host':
        return 'host';
      case 'Authorization':
      case 'authorization':
        return 'authorization';
      case 'Proxy-Authorization':
      case 'proxy-authorization':
        return 'proxy-authorization';
      case 'If-Modified-Since':
      case 'if-modified-since':
        return 'if-modified-since';
      case 'If-Unmodified-Since':
      case 'if-unmodified-since':
        return 'if-unmodified-since';
      case 'From':
      case 'from':
        return 'from';
      case 'Location':
      case 'location':
        return 'location';
      case 'Max-Forwards':
      case 'max-forwards':
        return 'max-forwards';
      case 'Retry-After':
      case 'retry-after':
        return 'retry-after';
      case 'ETag':
      case 'etag':
        return 'etag';
      case 'Last-Modified':
      case 'last-modified':
        return 'last-modified';
      case 'Server':
      case 'server':
        return 'server';
      case 'Age':
      case 'age':
        return 'age';
      case 'Expires':
      case 'expires':
        return 'expires';
      case 'Set-Cookie':
      case 'set-cookie':
        return '\u0001';
      case 'Cookie':
      case 'cookie':
        return '\u0002cookie';
      // The fields below are not used in _addHeaderLine(), but they are common
      // headers where we can avoid toLowerCase() if the mixed or lower case
      // versions match the first time through.
      case 'Transfer-Encoding':
      case 'transfer-encoding':
        return '\u0000transfer-encoding';
      case 'Date':
      case 'date':
        return '\u0000date';
      case 'Connection':
      case 'connection':
        return '\u0000connection';
      case 'Cache-Control':
      case 'cache-control':
        return '\u0000cache-control';
      case 'Vary':
      case 'vary':
        return '\u0000vary';
      case 'Content-Encoding':
      case 'content-encoding':
        return '\u0000content-encoding';
      case 'Origin':
      case 'origin':
        return '\u0000origin';
      case 'Upgrade':
      case 'upgrade':
        return '\u0000upgrade';
      case 'Expect':
      case 'expect':
        return '\u0000expect';
      case 'If-Match':
      case 'if-match':
        return '\u0000if-match';
      case 'If-None-Match':
      case 'if-none-match':
        return '\u0000if-none-match';
      case 'Accept':
      case 'accept':
        return '\u0000accept';
      case 'Accept-Encoding':
      case 'accept-encoding':
        return '\u0000accept-encoding';
      case 'Accept-Language':
      case 'accept-language':
        return '\u0000accept-language';
      case 'X-Forwarded-For':
      case 'x-forwarded-for':
        return '\u0000x-forwarded-for';
      case 'X-Forwarded-Host':
      case 'x-forwarded-host':
        return '\u0000x-forwarded-host';
      case 'X-Forwarded-Proto':
      case 'x-forwarded-proto':
        return '\u0000x-forwarded-proto';
      default:
        if (low)
          return '\u0000' + field;
        field = field.toLowerCase();
        low = true;
    }
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
