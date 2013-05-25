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

// an empty buffer for UPGRADE/CONNECT bodyHead compatibility
var emptyBuffer = new Buffer(0);

var common = require('_http_common');

var httpSocketSetup = common.httpSocketSetup;
var parsers = common.parsers;
var freeParser = common.freeParser;
var debug = common.debug;

var IncomingMessage = require('_http_incoming').IncomingMessage;
var OutgoingMessage = require('_http_outgoing').OutgoingMessage;

var agent = require('_http_agent');
var Agent = agent.Agent;
var globalAgent = agent.globalAgent;


function ClientRequest(options, cb) {
  var self = this;
  OutgoingMessage.call(self);

  self.agent = options.agent === undefined ? globalAgent : options.agent;

  var defaultPort = options.defaultPort || 80;

  var port = options.port || defaultPort;
  var host = options.hostname || options.host || 'localhost';

  if (options.setHost === undefined) {
    var setHost = true;
  }

  self.socketPath = options.socketPath;

  var method = self.method = (options.method || 'GET').toUpperCase();
  self.path = options.path || '/';
  if (cb) {
    self.once('response', cb);
  }

  if (!Array.isArray(options.headers)) {
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

  if (Array.isArray(options.headers)) {
    self._storeHeader(self.method + ' ' + self.path + ' HTTP/1.1\r\n',
                      options.headers);
  } else if (self.getHeader('expect')) {
    self._storeHeader(self.method + ' ' + self.path + ' HTTP/1.1\r\n',
                      self._renderHeaders());
  }
  if (self.socketPath) {
    self._last = true;
    self.shouldKeepAlive = false;
    if (options.createConnection) {
      self.onSocket(options.createConnection(self.socketPath));
    } else {
      self.onSocket(net.createConnection(self.socketPath));
    }
  } else if (self.agent) {
    // If there is an agent we should default to Connection:keep-alive.
    self._last = false;
    self.shouldKeepAlive = true;
    self.agent.addRequest(self, host, port, options.localAddress);
  } else {
    // No agent, default to Connection:close.
    self._last = true;
    self.shouldKeepAlive = false;
    if (options.createConnection) {
      options.port = port;
      options.host = host;
      var conn = options.createConnection(options);
    } else {
      var conn = net.createConnection({
        port: port,
        host: host,
        localAddress: options.localAddress
      });
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

ClientRequest.prototype._implicitHeader = function() {
  this._storeHeader(this.method + ' ' + this.path + ' HTTP/1.1\r\n',
                    this._renderHeaders());
};

ClientRequest.prototype.abort = function() {
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
  req.emit('close');
  if (req.res && req.res.readable) {
    // Socket closed before we emitted 'end' below.
    req.res.emit('aborted');
    var res = req.res;
    res.on('end', function() {
      res.emit('close');
    });
    res.push(null);
  } else if (!req.res && !req._hadError) {
    // This socket error fired before we started to
    // receive a response. The error needs to
    // fire on the request.
    req.emit('error', createHangUpError());
    req._hadError = true;
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
    req._hadError = true;
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

  if (!req.res) {
    // If we don't have a response then we know that the socket
    // ended prematurely and we need to emit an error on the request.
    req.emit('error', createHangUpError());
    req._hadError = true;
  }
  if (parser) {
    parser.finish();
    freeParser(parser, req);
  }
  socket.destroy();
}

function socketOnData(d, start, end) {
  var socket = this;
  var req = this._httpMessage;
  var parser = this.parser;

  var ret = parser.execute(d, start, end - start);
  if (ret instanceof Error) {
    debug('parse error');
    freeParser(parser, req);
    socket.destroy();
    req.emit('error', ret);
    req._hadError = true;
  } else if (parser.incoming && parser.incoming.upgrade) {
    // Upgrade or CONNECT
    var bytesParsed = ret;
    var res = parser.incoming;
    req.res = res;

    socket.ondata = null;
    socket.onend = null;
    parser.finish();

    // This is start + byteParsed
    var bodyHead = d.slice(start + bytesParsed, end);

    var eventName = req.method === 'CONNECT' ? 'connect' : 'upgrade';
    if (EventEmitter.listenerCount(req, eventName) > 0) {
      req.upgradeOrConnect = true;

      // detach the socket
      socket.emit('agentRemove');
      socket.removeListener('close', socketCloseListener);
      socket.removeListener('error', socketErrorListener);

      socket.unshift(bodyHead);

      req.emit(eventName, res, socket, emptyBuffer);
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
    freeParser(parser, req);
  }
}


// client
function parserOnIncomingClient(res, shouldKeepAlive) {
  var parser = this;
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

ClientRequest.prototype.onSocket = function(socket) {
  var req = this;

  process.nextTick(function() {
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
    if (typeof req.maxHeadersCount === 'number') {
      parser.maxHeaderPairs = req.maxHeadersCount << 1;
    } else {
      // Set default value because parser may be reused from FreeList
      parser.maxHeaderPairs = 2000;
    }

    socket.on('error', socketErrorListener);
    socket.ondata = socketOnData;
    socket.onend = socketOnEnd;
    socket.on('close', socketCloseListener);
    parser.onIncoming = parserOnIncomingClient;
    req.emit('socket', socket);
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
