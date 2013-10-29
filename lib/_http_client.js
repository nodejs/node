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

var util = require('util');
var net = require('net');
var EventEmitter = require('events').EventEmitter;
var HTTPParser = process.binding('http_parser').HTTPParser;
var assert = require('assert').ok;

var common = require('_http_common');

var httpSocketSetup = common.httpSocketSetup;
var parsers = common.parsers;
var freeParser = common.freeParser;
var debug = common.debug;

var IncomingMessage = require('_http_incoming').IncomingMessage;
var OutgoingMessage = require('_http_outgoing').OutgoingMessage;

var agent = require('_http_agent');
var globalAgent = agent.globalAgent;


function ClientRequest(options, cb) {
  var self = this;
  OutgoingMessage.call(self);

  options = util._extend({}, options);

  self.agent = util.isUndefined(options.agent) ? globalAgent : options.agent;

  var defaultPort = options.defaultPort || self.agent.defaultPort;

  var port = options.port = options.port || defaultPort;
  var host = options.host = options.hostname || options.host || 'localhost';

  if (util.isUndefined(options.setHost)) {
    var setHost = true;
  }

  self.socketPath = options.socketPath;

  var method = self.method = (options.method || 'GET').toUpperCase();
  self.path = options.path || '/';
  if (cb) {
    self.once('response', cb);
  }

  if (!util.isArray(options.headers)) {
    if (options.headers) {
      var keys = Object.keys(options.headers);
      for (var i = 0, l = keys.length; i < l; i++) {
        var key = keys[i];
        self.setHeader(key, options.headers[key]);
      }
    }
    if (host && !this.getHeader('host') && setHost) {
      var hostHeader = host;
      if (port && +port !== defaultPort) {
        hostHeader += ':' + port;
      }
      this.setHeader('Host', hostHeader);
    }
  }

  if (options.auth && !this.getHeader('Authorization')) {
    //basic auth
    this.setHeader('Authorization', 'Basic ' +
                   new Buffer(options.auth).toString('base64'));
  }

  if (method === 'GET' || method === 'HEAD' || method === 'CONNECT') {
    self.useChunkedEncodingByDefault = false;
  } else {
    self.useChunkedEncodingByDefault = true;
  }

  if (util.isArray(options.headers)) {
    self._storeHeader(self.method + ' ' + self.path + ' HTTP/1.1\r\n',
                      options.headers);
  } else if (self.getHeader('expect')) {
    self._storeHeader(self.method + ' ' + self.path + ' HTTP/1.1\r\n',
                      self._renderHeaders());
  }

  if (self.socketPath) {
    self._last = true;
    self.shouldKeepAlive = false;
    var conn = self.agent.createConnection({ path: self.socketPath });
    self.onSocket(conn);
  } else if (self.agent) {
    // If there is an agent we should default to Connection:keep-alive,
    // but only if the Agent will actually reuse the connection!
    // If it's not a keepAlive agent, and the maxSockets==Infinity, then
    // there's never a case where this socket will actually be reused
    var agent = self.agent;
    if (!agent.keepAlive && !Number.isFinite(agent.maxSockets)) {
      self._last = true;
      self.shouldKeepAlive = false;
    } else {
      self._last = false;
      self.shouldKeepAlive = true;
    }
    self.agent.addRequest(self, options);
  } else {
    // No agent, default to Connection:close.
    self._last = true;
    self.shouldKeepAlive = false;
    if (options.createConnection) {
      var conn = options.createConnection(options);
    } else {
      debug('CLIENT use net.createConnection', options);
      var conn = net.createConnection(options);
    }
    self.onSocket(conn);
  }

  self._deferToConnect(null, null, function() {
    self._flush();
    self = null;
  });
}

util.inherits(ClientRequest, OutgoingMessage);

exports.ClientRequest = ClientRequest;

ClientRequest.prototype._finish = function() {
  DTRACE_HTTP_CLIENT_REQUEST(this, this.connection);
  COUNTER_HTTP_CLIENT_REQUEST();
  OutgoingMessage.prototype._finish.call(this);
};

ClientRequest.prototype._implicitHeader = function() {
  this._storeHeader(this.method + ' ' + this.path + ' HTTP/1.1\r\n',
                    this._renderHeaders());
};

ClientRequest.prototype.abort = function() {
  // If we're aborting, we don't care about any more response data.
  if (this.res)
    this.res._dump();
  else
    this.once('response', function(res) {
      res._dump();
    });

  if (this.socket) {
    // in-progress
    this.socket.destroy();
  } else {
    // haven't been assigned a socket yet.
    // this could be more efficient, it could
    // remove itself from the pending requests
    this._deferToConnect('destroy', []);
  }
};


function createHangUpError() {
  var error = new Error('socket hang up');
  error.code = 'ECONNRESET';
  return error;
}


function socketCloseListener() {
  var socket = this;
  var parser = socket.parser;
  var req = socket._httpMessage;
  debug('HTTP socket close');

  // Pull through final chunk, if anything is buffered.
  // the ondata function will handle it properly, and this
  // is a no-op if no final chunk remains.
  socket.read();

  req.emit('close');
  if (req.res && req.res.readable) {
    // Socket closed before we emitted 'end' below.
    req.res.emit('aborted');
    var res = req.res;
    res.on('end', function() {
      res.emit('close');
    });
    res.push(null);
  } else if (!req.res && !req.socket._hadError) {
    // This socket error fired before we started to
    // receive a response. The error needs to
    // fire on the request.
    req.emit('error', createHangUpError());
    req.socket._hadError = true;
  }

  // Too bad.  That output wasn't getting written.
  // This is pretty terrible that it doesn't raise an error.
  // Fixed better in v0.10
  if (req.output)
    req.output.length = 0;
  if (req.outputEncodings)
    req.outputEncodings.length = 0;

  if (parser) {
    parser.finish();
    freeParser(parser, req);
  }
}

function socketErrorListener(err) {
  var socket = this;
  var parser = socket.parser;
  var req = socket._httpMessage;
  debug('SOCKET ERROR:', err.message, err.stack);

  if (req) {
    req.emit('error', err);
    // For Safety. Some additional errors might fire later on
    // and we need to make sure we don't double-fire the error event.
    req.socket._hadError = true;
  }

  if (parser) {
    parser.finish();
    freeParser(parser, req);
  }
  socket.destroy();
}

function socketOnEnd() {
  var socket = this;
  var req = this._httpMessage;
  var parser = this.parser;

  if (!req.res && !req.socket._hadError) {
    // If we don't have a response then we know that the socket
    // ended prematurely and we need to emit an error on the request.
    req.emit('error', createHangUpError());
    req.socket._hadError = true;
  }
  if (parser) {
    parser.finish();
    freeParser(parser, req);
  }
  socket.destroy();
}

function socketOnData(d) {
  var socket = this;
  var req = this._httpMessage;
  var parser = this.parser;

  assert(parser);

  var ret = parser.execute(d);
  if (ret instanceof Error) {
    debug('parse error');
    freeParser(parser, req);
    socket.destroy();
    req.emit('error', ret);
    req.socket._hadError = true;
  } else if (parser.incoming && parser.incoming.upgrade) {
    // Upgrade or CONNECT
    var bytesParsed = ret;
    var res = parser.incoming;
    req.res = res;

    socket.removeListener('data', socketOnData);
    socket.removeListener('end', socketOnEnd);
    parser.finish();

    var bodyHead = d.slice(bytesParsed, d.length);

    var eventName = req.method === 'CONNECT' ? 'connect' : 'upgrade';
    if (EventEmitter.listenerCount(req, eventName) > 0) {
      req.upgradeOrConnect = true;

      // detach the socket
      socket.emit('agentRemove');
      socket.removeListener('close', socketCloseListener);
      socket.removeListener('error', socketErrorListener);

      // TODO(isaacs): Need a way to reset a stream to fresh state
      // IE, not flowing, and not explicitly paused.
      socket._readableState.flowing = null;

      req.emit(eventName, res, socket, bodyHead);
      req.emit('close');
    } else {
      // Got Upgrade header or CONNECT method, but have no handler.
      socket.destroy();
    }
    freeParser(parser, req);
  } else if (parser.incoming && parser.incoming.complete &&
             // When the status code is 100 (Continue), the server will
             // send a final response after this client sends a request
             // body. So, we must not free the parser.
             parser.incoming.statusCode !== 100) {
    socket.removeListener('data', socketOnData);
    socket.removeListener('end', socketOnEnd);
    freeParser(parser, req);
  }
}


// client
function parserOnIncomingClient(res, shouldKeepAlive) {
  var socket = this.socket;
  var req = socket._httpMessage;


  // propogate "domain" setting...
  if (req.domain && !res.domain) {
    debug('setting "res.domain"');
    res.domain = req.domain;
  }

  debug('AGENT incoming response!');

  if (req.res) {
    // We already have a response object, this means the server
    // sent a double response.
    socket.destroy();
    return;
  }
  req.res = res;

  // Responses to CONNECT request is handled as Upgrade.
  if (req.method === 'CONNECT') {
    res.upgrade = true;
    return true; // skip body
  }

  // Responses to HEAD requests are crazy.
  // HEAD responses aren't allowed to have an entity-body
  // but *can* have a content-length which actually corresponds
  // to the content-length of the entity-body had the request
  // been a GET.
  var isHeadResponse = req.method == 'HEAD';
  debug('AGENT isHeadResponse', isHeadResponse);

  if (res.statusCode == 100) {
    // restart the parser, as this is a continue message.
    delete req.res; // Clear res so that we don't hit double-responses.
    req.emit('continue');
    return true;
  }

  if (req.shouldKeepAlive && !shouldKeepAlive && !req.upgradeOrConnect) {
    // Server MUST respond with Connection:keep-alive for us to enable it.
    // If we've been upgraded (via WebSockets) we also shouldn't try to
    // keep the connection open.
    req.shouldKeepAlive = false;
  }


  DTRACE_HTTP_CLIENT_RESPONSE(socket, req);
  COUNTER_HTTP_CLIENT_RESPONSE();
  req.res = res;
  res.req = req;

  // add our listener first, so that we guarantee socket cleanup
  res.on('end', responseOnEnd);
  var handled = req.emit('response', res);

  // If the user did not listen for the 'response' event, then they
  // can't possibly read the data, so we ._dump() it into the void
  // so that the socket doesn't hang there in a paused state.
  if (!handled)
    res._dump();

  return isHeadResponse;
}

// client
function responseOnEnd() {
  var res = this;
  var req = res.req;
  var socket = req.socket;

  if (!req.shouldKeepAlive) {
    if (socket.writable) {
      debug('AGENT socket.destroySoon()');
      socket.destroySoon();
    }
    assert(!socket.writable);
  } else {
    debug('AGENT socket keep-alive');
    if (req.timeoutCb) {
      socket.setTimeout(0, req.timeoutCb);
      req.timeoutCb = null;
    }
    socket.removeListener('close', socketCloseListener);
    socket.removeListener('error', socketErrorListener);
    // Mark this socket as available, AFTER user-added end
    // handlers have a chance to run.
    process.nextTick(function() {
      socket.emit('free');
    });
  }
}

function tickOnSocket(req, socket) {
  var parser = parsers.alloc();
  req.socket = socket;
  req.connection = socket;
  parser.reinitialize(HTTPParser.RESPONSE);
  parser.socket = socket;
  parser.incoming = null;
  req.parser = parser;

  socket.parser = parser;
  socket._httpMessage = req;

  // Setup "drain" propogation.
  httpSocketSetup(socket);

  // Propagate headers limit from request object to parser
  if (util.isNumber(req.maxHeadersCount)) {
    parser.maxHeaderPairs = req.maxHeadersCount << 1;
  } else {
    // Set default value because parser may be reused from FreeList
    parser.maxHeaderPairs = 2000;
  }

  parser.onIncoming = parserOnIncomingClient;
  socket.on('error', socketErrorListener);
  socket.on('data', socketOnData);
  socket.on('end', socketOnEnd);
  socket.on('close', socketCloseListener);
  req.emit('socket', socket);
}

ClientRequest.prototype.onSocket = function(socket) {
  var req = this;

  process.nextTick(function() {
    // If a domain was added to the request, attach it to the socket.
    if (req.domain)
      socket._handle.addAsyncListener(req.domain._listener);
    tickOnSocket(req, socket);
  });

};

ClientRequest.prototype._deferToConnect = function(method, arguments_, cb) {
  // This function is for calls that need to happen once the socket is
  // connected and writable. It's an important promisy thing for all the socket
  // calls that happen either now (when a socket is assigned) or
  // in the future (when a socket gets assigned out of the pool and is
  // eventually writable).
  var self = this;
  var onSocket = function() {
    if (self.socket.writable) {
      if (method) {
        self.socket[method].apply(self.socket, arguments_);
      }
      if (cb) { cb(); }
    } else {
      self.socket.once('connect', function() {
        if (method) {
          self.socket[method].apply(self.socket, arguments_);
        }
        if (cb) { cb(); }
      });
    }
  }
  if (!self.socket) {
    self.once('socket', onSocket);
  } else {
    onSocket();
  }
};

ClientRequest.prototype.setTimeout = function(msecs, callback) {
  if (callback) this.once('timeout', callback);

  var self = this;
  function emitTimeout() {
    self.emit('timeout');
  }

  if (this.socket && this.socket.writable) {
    if (this.timeoutCb)
      this.socket.setTimeout(0, this.timeoutCb);
    this.timeoutCb = emitTimeout;
    this.socket.setTimeout(msecs, emitTimeout);
    return;
  }

  // Set timeoutCb so that it'll get cleaned up on request end
  this.timeoutCb = emitTimeout;
  if (this.socket) {
    var sock = this.socket;
    this.socket.once('connect', function() {
      sock.setTimeout(msecs, emitTimeout);
    });
    return;
  }

  this.once('socket', function(sock) {
    sock.setTimeout(msecs, emitTimeout);
  });
};

ClientRequest.prototype.setNoDelay = function() {
  this._deferToConnect('setNoDelay', arguments);
};
ClientRequest.prototype.setSocketKeepAlive = function() {
  this._deferToConnect('setKeepAlive', arguments);
};

ClientRequest.prototype.clearTimeout = function(cb) {
  this.setTimeout(0, cb);
};
