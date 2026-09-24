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

const {
  ObjectDefineProperty,
  ObjectSetPrototypeOf,
  Symbol,
} = primordials;

const { Readable, finished } = require('stream');
const {
  deprecateInstantiation,
} = require('internal/util');

const { AbortController } = require('internal/abort_controller');

const kHeaders = Symbol('kHeaders');
const kHeadersDistinct = Symbol('kHeadersDistinct');
const kHeadersCount = Symbol('kHeadersCount');
const kTrailers = Symbol('kTrailers');
const kTrailersDistinct = Symbol('kTrailersDistinct');
const kTrailersCount = Symbol('kTrailersCount');
const kAbortController = Symbol('kAbortController');
const kAbortSignalSocket = Symbol('kAbortSignalSocket');
const kAbortSignalListener = Symbol('kAbortSignalListener');
const kAbortSignalDetached = Symbol('kAbortSignalDetached');
const kAttachAbortSignal = Symbol('kAttachAbortSignal');
const kDetachAbortSignal = Symbol('kDetachAbortSignal');

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
  if (!(this instanceof IncomingMessage)) {
    return deprecateInstantiation(IncomingMessage, 'DEP0195', socket);
  }

  let streamOptions;

  if (socket) {
    streamOptions = {
      highWaterMark: socket.readableHighWaterMark,
    };
  }

  Readable.call(this, streamOptions);

  this._readableState.readingMore = true;

  this.socket = socket;

  this.httpVersionMajor = null;
  this.httpVersionMinor = null;
  this.httpVersion = null;
  this.complete = false;
  this[kHeaders] = null;
  this[kHeadersCount] = 0;
  this.rawHeaders = [];
  this[kTrailers] = null;
  this[kTrailersCount] = 0;
  this.rawTrailers = [];
  this.joinDuplicateHeaders = false;
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
  this[kAbortController] = null;
  this[kAbortSignalSocket] = null;
  this[kAbortSignalListener] = null;
  this[kAbortSignalDetached] = false;
}
ObjectSetPrototypeOf(IncomingMessage.prototype, Readable.prototype);
ObjectSetPrototypeOf(IncomingMessage, Readable);

ObjectDefineProperty(IncomingMessage.prototype, 'connection', {
  __proto__: null,
  get: function() {
    return this.socket;
  },
  set: function(val) {
    this.socket = val;
  },
});

ObjectDefineProperty(IncomingMessage.prototype, 'headers', {
  __proto__: null,
  get: function() {
    if (!this[kHeaders]) {
      this[kHeaders] = { __proto__: null };

      const src = this.rawHeaders;
      const dst = this[kHeaders];

      for (let n = 0; n < this[kHeadersCount]; n += 2) {
        this._addHeaderLine(src[n + 0], src[n + 1], dst);
      }
    }
    return this[kHeaders];
  },
  set: function(val) {
    this[kHeaders] = val;
  },
});

ObjectDefineProperty(IncomingMessage.prototype, 'headersDistinct', {
  __proto__: null,
  get: function() {
    if (!this[kHeadersDistinct]) {
      this[kHeadersDistinct] = { __proto__: null };

      const src = this.rawHeaders;
      const dst = this[kHeadersDistinct];

      for (let n = 0; n < this[kHeadersCount]; n += 2) {
        this._addHeaderLineDistinct(src[n + 0], src[n + 1], dst);
      }
    }
    return this[kHeadersDistinct];
  },
  set: function(val) {
    this[kHeadersDistinct] = val;
  },
});

ObjectDefineProperty(IncomingMessage.prototype, 'trailers', {
  __proto__: null,
  get: function() {
    if (!this[kTrailers]) {
      this[kTrailers] = { __proto__: null };

      const src = this.rawTrailers;
      const dst = this[kTrailers];

      for (let n = 0; n < this[kTrailersCount]; n += 2) {
        this._addHeaderLine(src[n + 0], src[n + 1], dst);
      }
    }
    return this[kTrailers];
  },
  set: function(val) {
    this[kTrailers] = val;
  },
});

ObjectDefineProperty(IncomingMessage.prototype, 'trailersDistinct', {
  __proto__: null,
  get: function() {
    if (!this[kTrailersDistinct]) {
      this[kTrailersDistinct] = { __proto__: null };

      const src = this.rawTrailers;
      const dst = this[kTrailersDistinct];

      for (let n = 0; n < this[kTrailersCount]; n += 2) {
        this._addHeaderLineDistinct(src[n + 0], src[n + 1], dst);
      }
    }
    return this[kTrailersDistinct];
  },
  set: function(val) {
    this[kTrailersDistinct] = val;
  },
});

ObjectDefineProperty(IncomingMessage.prototype, 'signal', {
  __proto__: null,
  configurable: true,
  get: function() {
    if (this[kAbortController] === null) {
      const ac = new AbortController();
      this[kAbortController] = ac;
      if (this.destroyed && (!this.readableEnded || !this.complete)) {
        ac.abort();
      } else {
        this[kAttachAbortSignal]();
      }
    }
    return this[kAbortController].signal;
  },
});

IncomingMessage.prototype[kAttachAbortSignal] = function() {
  if (this[kAbortController].signal.aborted ||
      this[kAbortSignalDetached] ||
      this[kAbortSignalListener] !== null) {
    return;
  }

  const socket = this.socket;
  if (!socket) {
    return;
  }

  if (socket.destroyed) {
    abortSignal(this);
    return;
  }

  this[kAbortSignalSocket] = socket;
  this[kAbortSignalListener] = () => {
    abortSignal(this);
  };
  socket.once('close', this[kAbortSignalListener]);
};

IncomingMessage.prototype[kDetachAbortSignal] = function() {
  const socket = this[kAbortSignalSocket];
  const listener = this[kAbortSignalListener];
  this[kAbortSignalDetached] = true;
  this[kAbortSignalSocket] = null;
  this[kAbortSignalListener] = null;
  if (socket !== null && listener !== null) {
    socket.removeListener('close', listener);
  }
};

IncomingMessage.prototype.setTimeout = function setTimeout(msecs, callback) {
  if (callback)
    this.on('timeout', callback);
  this.socket.setTimeout(msecs);
  return this;
};

IncomingMessage.prototype._read = function _read() {
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
IncomingMessage.prototype._destroy = function _destroy(err, cb) {
  if (!this.readableEnded || !this.complete) {
    this.aborted = true;
    this.emit('aborted');
    abortSignal(this);
  }

  // If aborted and the underlying socket is not already destroyed,
  // destroy it.
  // We have to check if the socket is already destroyed because finished
  // does not call the callback when this method is invoked from `_http_client`
  // in `test/parallel/test-http-client-spurious-aborted.js`
  if (this.socket && !this.socket.destroyed && this.aborted) {
    this.socket.destroy(err);
    const cleanup = finished(this.socket, (e) => {
      if (e?.code === 'ERR_STREAM_PREMATURE_CLOSE') {
        e = null;
      }
      cleanup();
      process.nextTick(onError, this, e || err, cb);
    });
  } else if (err == null && !this.aborted) {
    // The message was received completely and is being destroyed cleanly:
    // complete the destroy synchronously. 'close' is still emitted on a
    // later tick by the stream machinery. The deferral below only exists
    // so that 'error' listeners attached right after destroy(err) still
    // receive the error, which cannot matter when there is no error.
    cb();
  } else {
    process.nextTick(onError, this, err, cb);
  }
};

function abortSignal(self) {
  self[kDetachAbortSignal]();
  if (self[kAbortController] !== null) {
    self[kAbortController].abort();
  }
}

IncomingMessage.prototype._addHeaderLines = _addHeaderLines;
function _addHeaderLines(headers, n) {
  if (headers?.length) {
    let dest;
    if (this.complete) {
      this.rawTrailers = headers;
      this[kTrailersCount] = n;
      dest = this[kTrailers];
    } else {
      this.rawHeaders = headers;
      this[kHeadersCount] = n;
      dest = this[kHeaders];
    }

    if (dest) {
      for (let i = 0; i < n; i += 2) {
        this._addHeaderLine(headers[i], headers[i + 1], dest);
      }
    }
  }
}


// How repeated instances of a header field are combined in the headers
// object. Fields that are not known are joined with ', '.
const kFirstWins = 0;
const kJoinComma = 1;
const kJoinSemicolon = 2;
const kArray = 3;

function knownField(name, merge) {
  return { name, merge };
}

// Later values are dropped, unless joinDuplicateHeaders is set.
const kAge = knownField('age', kFirstWins);
const kHost = knownField('host', kFirstWins);
const kFrom = knownField('from', kFirstWins);
const kETag = knownField('etag', kFirstWins);
const kServer = knownField('server', kFirstWins);
const kReferer = knownField('referer', kFirstWins);
const kExpires = knownField('expires', kFirstWins);
const kLocation = knownField('location', kFirstWins);
const kUserAgent = knownField('user-agent', kFirstWins);
const kRetryAfter = knownField('retry-after', kFirstWins);
const kContentType = knownField('content-type', kFirstWins);
const kMaxForwards = knownField('max-forwards', kFirstWins);
const kAuthorization = knownField('authorization', kFirstWins);
const kLastModified = knownField('last-modified', kFirstWins);
const kContentLength = knownField('content-length', kFirstWins);
const kIfModifiedSince = knownField('if-modified-since', kFirstWins);
const kProxyAuthorization = knownField('proxy-authorization', kFirstWins);
const kIfUnmodifiedSince = knownField('if-unmodified-since', kFirstWins);

// Values are joined with ', '.
const kDate = knownField('date', kJoinComma);
const kVary = knownField('vary', kJoinComma);
const kOrigin = knownField('origin', kJoinComma);
const kExpect = knownField('expect', kJoinComma);
const kAccept = knownField('accept', kJoinComma);
const kUpgrade = knownField('upgrade', kJoinComma);
const kIfMatch = knownField('if-match', kJoinComma);
const kConnection = knownField('connection', kJoinComma);
const kCacheControl = knownField('cache-control', kJoinComma);
const kIfNoneMatch = knownField('if-none-match', kJoinComma);
const kAcceptEncoding = knownField('accept-encoding', kJoinComma);
const kAcceptLanguage = knownField('accept-language', kJoinComma);
const kXForwardedFor = knownField('x-forwarded-for', kJoinComma);
const kContentEncoding = knownField('content-encoding', kJoinComma);
const kXForwardedHost = knownField('x-forwarded-host', kJoinComma);
const kTransferEncoding = knownField('transfer-encoding', kJoinComma);
const kXForwardedProto = knownField('x-forwarded-proto', kJoinComma);

// Values are joined with '; '.
const kCookie = knownField('cookie', kJoinSemicolon);

// Values are collected into an array.
const kSetCookie = knownField('set-cookie', kArray);

// Returns the descriptor of a known field, or the lowercased name of any other
// field. The 'traditional cased' and lowercase spellings of known fields are
// matched first to avoid calling toLowerCase() for them.
// TODO: perhaps http_parser could be returning both raw and lowercased versions
// of known header names to avoid us having to call toLowerCase() for those
// headers.
function matchKnownFields(field, lowercased) {
  switch (field.length) {
    case 3:
      if (field === 'Age' || field === 'age') return kAge;
      break;
    case 4:
      if (field === 'Host' || field === 'host') return kHost;
      if (field === 'From' || field === 'from') return kFrom;
      if (field === 'ETag' || field === 'etag') return kETag;
      if (field === 'Date' || field === 'date') return kDate;
      if (field === 'Vary' || field === 'vary') return kVary;
      break;
    case 6:
      if (field === 'Server' || field === 'server') return kServer;
      if (field === 'Cookie' || field === 'cookie') return kCookie;
      if (field === 'Origin' || field === 'origin') return kOrigin;
      if (field === 'Expect' || field === 'expect') return kExpect;
      if (field === 'Accept' || field === 'accept') return kAccept;
      break;
    case 7:
      if (field === 'Referer' || field === 'referer') return kReferer;
      if (field === 'Expires' || field === 'expires') return kExpires;
      if (field === 'Upgrade' || field === 'upgrade') return kUpgrade;
      break;
    case 8:
      if (field === 'Location' || field === 'location')
        return kLocation;
      if (field === 'If-Match' || field === 'if-match')
        return kIfMatch;
      break;
    case 10:
      if (field === 'User-Agent' || field === 'user-agent')
        return kUserAgent;
      if (field === 'Set-Cookie' || field === 'set-cookie')
        return kSetCookie;
      if (field === 'Connection' || field === 'connection')
        return kConnection;
      break;
    case 11:
      if (field === 'Retry-After' || field === 'retry-after')
        return kRetryAfter;
      break;
    case 12:
      if (field === 'Content-Type' || field === 'content-type')
        return kContentType;
      if (field === 'Max-Forwards' || field === 'max-forwards')
        return kMaxForwards;
      break;
    case 13:
      if (field === 'Authorization' || field === 'authorization')
        return kAuthorization;
      if (field === 'Last-Modified' || field === 'last-modified')
        return kLastModified;
      if (field === 'Cache-Control' || field === 'cache-control')
        return kCacheControl;
      if (field === 'If-None-Match' || field === 'if-none-match')
        return kIfNoneMatch;
      break;
    case 14:
      if (field === 'Content-Length' || field === 'content-length')
        return kContentLength;
      break;
    case 15:
      if (field === 'Accept-Encoding' || field === 'accept-encoding')
        return kAcceptEncoding;
      if (field === 'Accept-Language' || field === 'accept-language')
        return kAcceptLanguage;
      if (field === 'X-Forwarded-For' || field === 'x-forwarded-for')
        return kXForwardedFor;
      break;
    case 16:
      if (field === 'Content-Encoding' || field === 'content-encoding')
        return kContentEncoding;
      if (field === 'X-Forwarded-Host' || field === 'x-forwarded-host')
        return kXForwardedHost;
      break;
    case 17:
      if (field === 'If-Modified-Since' || field === 'if-modified-since')
        return kIfModifiedSince;
      if (field === 'Transfer-Encoding' || field === 'transfer-encoding')
        return kTransferEncoding;
      if (field === 'X-Forwarded-Proto' || field === 'x-forwarded-proto')
        return kXForwardedProto;
      break;
    case 19:
      if (field === 'Proxy-Authorization' || field === 'proxy-authorization')
        return kProxyAuthorization;
      if (field === 'If-Unmodified-Since' || field === 'if-unmodified-since')
        return kIfUnmodifiedSince;
      break;
  }
  if (lowercased) {
    return field;
  }
  return matchKnownFields(field.toLowerCase(), true);
}
// Add the given (field, value) pair to the message
//
// Per RFC2616, section 4.2 it is acceptable to join multiple instances of the
// same header with a ', ' if the header in question supports specification of
// multiple values this way. The one exception to this is the Cookie header,
// which has multiple values joined with a '; ' instead. If a header's values
// cannot be joined in either of these ways, we declare the first instance the
// winner and drop the second. Fields that are not known are always joined.
IncomingMessage.prototype._addHeaderLine = _addHeaderLine;
function _addHeaderLine(field, value, dest) {
  const known = matchKnownFields(field);
  let merge = kJoinComma;
  if (typeof known === 'string') {
    field = known;
  } else {
    field = known.name;
    merge = known.merge;
  }
  if (merge === kJoinComma || merge === kJoinSemicolon) {
    // Make a delimited list
    if (typeof dest[field] === 'string') {
      dest[field] += (merge === kJoinComma ? ', ' : '; ') + value;
    } else {
      dest[field] = value;
    }
  } else if (merge === kArray) {
    // Array header -- only Set-Cookie at the moment
    if (dest['set-cookie'] !== undefined) {
      dest['set-cookie'].push(value);
    } else {
      dest['set-cookie'] = [value];
    }
  } else if (this.joinDuplicateHeaders) {
    // RFC 9110 https://www.rfc-editor.org/rfc/rfc9110#section-5.2
    // https://github.com/nodejs/node/issues/45699
    // allow authorization multiple fields
    // Make a delimited list
    if (dest[field] === undefined) {
      dest[field] = value;
    } else {
      dest[field] += ', ' + value;
    }
  } else if (dest[field] === undefined) {
    // Drop duplicates
    dest[field] = value;
  }
}

IncomingMessage.prototype._addHeaderLineDistinct = _addHeaderLineDistinct;
function _addHeaderLineDistinct(field, value, dest) {
  field = field.toLowerCase();
  if (!dest[field]) {
    dest[field] = [value];
  } else {
    dest[field].push(value);
  }
}

IncomingMessage.prototype._dumpAndCloseReadable = function _dumpAndCloseReadable() {
  this._dumped = true;
  this._readableState.ended = true;
  this._readableState.endEmitted = true;
  this._readableState.destroyed = true;
  this._readableState.closed = true;
  this._readableState.closeEmitted = true;
};


// Call this instead of resume() if we want to just
// dump all the data to /dev/null
IncomingMessage.prototype._dump = function _dump() {
  if (!this._dumped) {
    this._dumped = true;
    // If there is buffered data, it may trigger 'data' events.
    // Remove 'data' event listeners explicitly.
    this.removeAllListeners('data');
    if (this.complete && !this.destroyed && this.readableLength === 0) {
      // The message was fully received and never read: there is nothing to
      // pull off the wire. Go straight to the 'end' emission instead of
      // paying for the resume() and flow() scheduling machinery.
      this.read(0);
    } else {
      this.resume();
    }
  }
};

function onError(self, error, cb) {
  // This is to keep backward compatible behavior.
  // An error is emitted only if there are listeners attached to the event.
  if (self.listenerCount('error') === 0) {
    cb();
  } else {
    cb(error);
  }
}

module.exports = {
  IncomingMessage,
  kDetachAbortSignal,
  kHeadersCount,
  readStart,
  readStop,
};
