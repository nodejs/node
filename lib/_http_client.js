'use strict';

const util = require('util');
const net = require('net');
const url = require('url');
const HTTPParser = process.binding('http_parser').HTTPParser;
const assert = require('assert').ok;
const common = require('_http_common');
const httpSocketSetup = common.httpSocketSetup;
const parsers = common.parsers;
const freeParser = common.freeParser;
const debug = common.debug;
const OutgoingMessage = require('_http_outgoing').OutgoingMessage;
const Agent = require('_http_agent');
const Buffer = require('buffer').Buffer;
const urlToOptions = require('internal/url').urlToOptions;

// The actual list of disallowed characters in regexp form is more like:
//    /[^A-Za-z0-9\-._~!$&'()*+,;=/:@]/
// with an additional rule for ignoring percentage-escaped characters, but
// that's a) hard to capture in a regular expression that performs well, and
// b) possibly too restrictive for real-world usage. So instead we restrict the
// filter to just control characters and spaces.
//
// This function is used in the case of small paths, where manual character code
// checks can greatly outperform the equivalent regexp (tested in V8 5.4).
function isInvalidPath(s) {
  var i = 0;
  if (s.charCodeAt(0) <= 32) return true;
  if (++i >= s.length) return false;
  if (s.charCodeAt(1) <= 32) return true;
  if (++i >= s.length) return false;
  if (s.charCodeAt(2) <= 32) return true;
  if (++i >= s.length) return false;
  if (s.charCodeAt(3) <= 32) return true;
  if (++i >= s.length) return false;
  if (s.charCodeAt(4) <= 32) return true;
  if (++i >= s.length) return false;
  if (s.charCodeAt(5) <= 32) return true;
  ++i;
  for (; i < s.length; ++i)
    if (s.charCodeAt(i) <= 32) return true;
  return false;
}

function ClientRequest(options, cb) {
  var self = this;
  OutgoingMessage.call(self);

  if (typeof options === 'string') {
    options = url.parse(options);
    if (!options.hostname) {
      throw new Error('Unable to determine the domain name');
    }
  } else if (options instanceof url.URL) {
    options = urlToOptions(options);
  } else {
    options = util._extend({}, options);
  }

  var agent = options.agent;
  var defaultAgent = options._defaultAgent || Agent.globalAgent;
  if (agent === false) {
    agent = new defaultAgent.constructor();
  } else if (agent === null || agent === undefined) {
    if (typeof options.createConnection !== 'function') {
      agent = defaultAgent;
    }
    // Explicitly pass through this statement as agent will not be used
    // when createConnection is provided.
  } else if (!(agent instanceof Agent.Agent)) {
    throw new TypeError(
      'Agent option must be an instance of http.Agent, undefined or false.'
    );
  }
  self.agent = agent;

  var protocol = options.protocol || defaultAgent.protocol;
  var expectedProtocol = defaultAgent.protocol;
  if (self.agent && self.agent.protocol)
    expectedProtocol = self.agent.protocol;

  var path;
  if (options.path) {
    path = '' + options.path;
    var invalidPath;
    if (path.length <= 39) { // Determined experimentally in V8 5.4
      invalidPath = isInvalidPath(path);
    } else {
      invalidPath = /[\u0000-\u0020]/.test(path);
    }
    if (invalidPath)
      throw new TypeError('Request path contains unescaped characters');
  }

  if (protocol !== expectedProtocol) {
    throw new Error('Protocol "' + protocol + '" not supported. ' +
                    'Expected "' + expectedProtocol + '"');
  }

  const defaultPort = options.defaultPort ||
                      self.agent && self.agent.defaultPort;

  var port = options.port = options.port || defaultPort || 80;
  var host = options.host = options.hostname || options.host || 'localhost';

  var setHost = (options.setHost === undefined);

  self.socketPath = options.socketPath;
  self.timeout = options.timeout;

  var method = options.method;
  var methodIsString = (typeof method === 'string');
  if (method != null && !methodIsString) {
    throw new TypeError('Method must be a string');
  }

  if (methodIsString && method) {
    if (!common._checkIsHttpToken(method)) {
      throw new TypeError('Method must be a valid HTTP token');
    }
    method = self.method = method.toUpperCase();
  } else {
    method = self.method = 'GET';
  }

  self.path = options.path || '/';
  if (cb) {
    self.once('response', cb);
  }

  var headersArray = Array.isArray(options.headers);
  if (!headersArray) {
    if (options.headers) {
      var keys = Object.keys(options.headers);
      for (var i = 0; i < keys.length; i++) {
        var key = keys[i];
        self.setHeader(key, options.headers[key]);
      }
    }
    if (host && !this.getHeader('host') && setHost) {
      var hostHeader = host;
      var posColon = -1;

      // For the Host header, ensure that IPv6 addresses are enclosed
      // in square brackets, as defined by URI formatting
      // https://tools.ietf.org/html/rfc3986#section-3.2.2
      if (-1 !== (posColon = hostHeader.indexOf(':')) &&
          -1 !== (posColon = hostHeader.indexOf(':', posColon)) &&
          '[' !== hostHeader[0]) {
        hostHeader = `[${hostHeader}]`;
      }

      if (port && +port !== defaultPort) {
        hostHeader += ':' + port;
      }
      this.setHeader('Host', hostHeader);
    }
  }

  if (options.auth && !this.getHeader('Authorization')) {
    this.setHeader('Authorization', 'Basic ' +
                   Buffer.from(options.auth).toString('base64'));
  }

  if (method === 'GET' ||
      method === 'HEAD' ||
      method === 'DELETE' ||
      method === 'OPTIONS' ||
      method === 'CONNECT') {
    self.useChunkedEncodingByDefault = false;
  } else {
    self.useChunkedEncodingByDefault = true;
  }

  if (headersArray) {
    self._storeHeader(self.method + ' ' + self.path + ' HTTP/1.1\r\n',
                      options.headers);
  } else if (self.getHeader('expect')) {
    if (self._header) {
      throw new Error('Can\'t render headers after they are sent to the ' +
                      'client');
    }
    self._storeHeader(self.method + ' ' + self.path + ' HTTP/1.1\r\n',
                      self._headers);
  }

  this._ended = false;
  this.res = null;
  this.aborted = undefined;
  this.timeoutCb = null;
  this.upgradeOrConnect = false;
  this.parser = null;
  this.maxHeadersCount = null;

  var called = false;
  if (self.socketPath) {
    self._last = true;
    self.shouldKeepAlive = false;
    const optionsPath = {
      path: self.socketPath,
      timeout: self.timeout
    };
    const newSocket = self.agent.createConnection(optionsPath, oncreate);
    if (newSocket && !called) {
      called = true;
      self.onSocket(newSocket);
    } else {
      return;
    }
  } else if (self.agent) {
    // If there is an agent we should default to Connection:keep-alive,
    // but only if the Agent will actually reuse the connection!
    // If it's not a keepAlive agent, and the maxSockets==Infinity, then
    // there's never a case where this socket will actually be reused
    if (!self.agent.keepAlive && !Number.isFinite(self.agent.maxSockets)) {
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
    if (typeof options.createConnection === 'function') {
      const newSocket = options.createConnection(options, oncreate);
      if (newSocket && !called) {
        called = true;
        self.onSocket(newSocket);
      } else {
        return;
      }
    } else {
      debug('CLIENT use net.createConnection', options);
      self.onSocket(net.createConnection(options));
    }
  }

  function oncreate(err, socket) {
    if (called)
      return;
    called = true;
    if (err) {
      process.nextTick(function() {
        self.emit('error', err);
      });
      return;
    }
    self.onSocket(socket);
    self._deferToConnect(null, null, function() {
      self._flush();
      self = null;
    });
  }

  self._deferToConnect(null, null, function() {
    self._flush();
    self = null;
  });
}

util.inherits(ClientRequest, OutgoingMessage);

exports.ClientRequest = ClientRequest;

ClientRequest.prototype._finish = function _finish() {
  DTRACE_HTTP_CLIENT_REQUEST(this, this.connection);
  LTTNG_HTTP_CLIENT_REQUEST(this, this.connection);
  COUNTER_HTTP_CLIENT_REQUEST();
  OutgoingMessage.prototype._finish.call(this);
};

ClientRequest.prototype._implicitHeader = function _implicitHeader() {
  if (this._header) {
    throw new Error('Can\'t render headers after they are sent to the client');
  }
  this._storeHeader(this.method + ' ' + this.path + ' HTTP/1.1\r\n',
                    this._headers);
};

ClientRequest.prototype.abort = function abort() {
  if (!this.aborted) {
    process.nextTick(emitAbortNT, this);
  }
  // Mark as aborting so we can avoid sending queued request data
  // This is used as a truthy flag elsewhere. The use of Date.now is for
  // debugging purposes only.
  this.aborted = Date.now();

  // If we're aborting, we don't care about any more response data.
  if (this.res)
    this.res._dump();
  else
    this.once('response', function(res) {
      res._dump();
    });

  // In the event that we don't have a socket, we will pop out of
  // the request queue through handling in onSocket.
  if (this.socket) {
    // in-progress
    this.socket.destroy();
  }
};


function emitAbortNT(self) {
  self.emit('abort');
}


function createHangUpError() {
  var error = new Error('socket hang up');
  error.code = 'ECONNRESET';
  return error;
}


function socketCloseListener() {
  var socket = this;
  var req = socket._httpMessage;
  debug('HTTP socket close');

  // Pull through final chunk, if anything is buffered.
  // the ondata function will handle it properly, and this
  // is a no-op if no final chunk remains.
  socket.read();

  // NOTE: It's important to get parser here, because it could be freed by
  // the `socketOnData`.
  var parser = socket.parser;
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
    freeParser(parser, req, socket);
  }
}

function socketErrorListener(err) {
  var socket = this;
  var req = socket._httpMessage;
  debug('SOCKET ERROR:', err.message, err.stack);

  if (req) {
    req.emit('error', err);
    // For Safety. Some additional errors might fire later on
    // and we need to make sure we don't double-fire the error event.
    req.socket._hadError = true;
  }

  // Handle any pending data
  socket.read();

  var parser = socket.parser;
  if (parser) {
    parser.finish();
    freeParser(parser, req, socket);
  }

  // Ensure that no further data will come out of the socket
  socket.removeListener('data', socketOnData);
  socket.removeListener('end', socketOnEnd);
  socket.destroy();
}

function freeSocketErrorListener(err) {
  var socket = this;
  debug('SOCKET ERROR on FREE socket:', err.message, err.stack);
  socket.destroy();
  socket.emit('agentRemove');
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
    freeParser(parser, req, socket);
  }
  socket.destroy();
}

function socketOnData(d) {
  var socket = this;
  var req = this._httpMessage;
  var parser = this.parser;

  assert(parser && parser.socket === socket);

  var ret = parser.execute(d);
  if (ret instanceof Error) {
    debug('parse error');
    freeParser(parser, req, socket);
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
    if (req.listenerCount(eventName) > 0) {
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
    freeParser(parser, req, socket);
  } else if (parser.incoming && parser.incoming.complete &&
             // When the status code is 100 (Continue), the server will
             // send a final response after this client sends a request
             // body. So, we must not free the parser.
             parser.incoming.statusCode !== 100) {
    socket.removeListener('data', socketOnData);
    socket.removeListener('end', socketOnEnd);
    freeParser(parser, req, socket);
  }
}


// client
function parserOnIncomingClient(res, shouldKeepAlive) {
  var socket = this.socket;
  var req = socket._httpMessage;


  // propagate "domain" setting...
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
    return 2; // skip body, and the rest
  }

  // Responses to HEAD requests are crazy.
  // HEAD responses aren't allowed to have an entity-body
  // but *can* have a content-length which actually corresponds
  // to the content-length of the entity-body had the request
  // been a GET.
  var isHeadResponse = req.method === 'HEAD';
  debug('AGENT isHeadResponse', isHeadResponse);

  if (res.statusCode === 100) {
    // restart the parser, as this is a continue message.
    req.res = null; // Clear res so that we don't hit double-responses.
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
  LTTNG_HTTP_CLIENT_RESPONSE(socket, req);
  COUNTER_HTTP_CLIENT_RESPONSE();
  req.res = res;
  res.req = req;

  // add our listener first, so that we guarantee socket cleanup
  res.on('end', responseOnEnd);
  req.on('prefinish', requestOnPrefinish);
  var handled = req.emit('response', res);

  // If the user did not listen for the 'response' event, then they
  // can't possibly read the data, so we ._dump() it into the void
  // so that the socket doesn't hang there in a paused state.
  if (!handled)
    res._dump();

  return isHeadResponse;
}

// client
function responseKeepAlive(res, req) {
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
    socket.once('error', freeSocketErrorListener);
    // Mark this socket as available, AFTER user-added end
    // handlers have a chance to run.
    process.nextTick(emitFreeNT, socket);
  }
}

function responseOnEnd() {
  const res = this;
  const req = this.req;

  req._ended = true;
  if (!req.shouldKeepAlive || req.finished)
    responseKeepAlive(res, req);
}

function requestOnPrefinish() {
  const req = this;
  const res = this.res;

  if (!req.shouldKeepAlive)
    return;

  if (req._ended)
    responseKeepAlive(res, req);
}

function emitFreeNT(socket) {
  socket.emit('free');
}

function tickOnSocket(req, socket) {
  var parser = parsers.alloc();
  req.socket = socket;
  req.connection = socket;
  parser.reinitialize(HTTPParser.RESPONSE);
  parser.socket = socket;
  parser.incoming = null;
  parser.outgoing = req;
  req.parser = parser;

  socket.parser = parser;
  socket._httpMessage = req;

  // Setup "drain" propagation.
  httpSocketSetup(socket);

  // Propagate headers limit from request object to parser
  if (typeof req.maxHeadersCount === 'number') {
    parser.maxHeaderPairs = req.maxHeadersCount << 1;
  } else {
    // Set default value because parser may be reused from FreeList
    parser.maxHeaderPairs = 2000;
  }

  parser.onIncoming = parserOnIncomingClient;
  socket.removeListener('error', freeSocketErrorListener);
  socket.on('error', socketErrorListener);
  socket.on('data', socketOnData);
  socket.on('end', socketOnEnd);
  socket.on('close', socketCloseListener);

  if (req.timeout) {
    const emitRequestTimeout = () => req.emit('timeout');
    socket.once('timeout', emitRequestTimeout);
    req.once('response', (res) => {
      res.once('end', () => {
        socket.removeListener('timeout', emitRequestTimeout);
      });
    });
  }
  req.emit('socket', socket);
}

ClientRequest.prototype.onSocket = function onSocket(socket) {
  process.nextTick(onSocketNT, this, socket);
};

function onSocketNT(req, socket) {
  if (req.aborted) {
    // If we were aborted while waiting for a socket, skip the whole thing.
    if (req.socketPath || !req.agent) {
      socket.destroy();
    } else {
      socket.emit('free');
    }
  } else {
    tickOnSocket(req, socket);
  }
}

ClientRequest.prototype._deferToConnect = _deferToConnect;
function _deferToConnect(method, arguments_, cb) {
  // This function is for calls that need to happen once the socket is
  // connected and writable. It's an important promisy thing for all the socket
  // calls that happen either now (when a socket is assigned) or
  // in the future (when a socket gets assigned out of the pool and is
  // eventually writable).
  var self = this;

  function callSocketMethod() {
    if (method)
      self.socket[method].apply(self.socket, arguments_);

    if (typeof cb === 'function')
      cb();
  }

  var onSocket = function onSocket() {
    if (self.socket.writable) {
      callSocketMethod();
    } else {
      self.socket.once('connect', callSocketMethod);
    }
  };

  if (!self.socket) {
    self.once('socket', onSocket);
  } else {
    onSocket();
  }
}

ClientRequest.prototype.setTimeout = function setTimeout(msecs, callback) {
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
    return this;
  }

  // Set timeoutCb so that it'll get cleaned up on request end
  this.timeoutCb = emitTimeout;
  if (this.socket) {
    var sock = this.socket;
    this.socket.once('connect', function() {
      sock.setTimeout(msecs, emitTimeout);
    });
    return this;
  }

  this.once('socket', function(sock) {
    sock.setTimeout(msecs, emitTimeout);
  });

  return this;
};

ClientRequest.prototype.setNoDelay = function setNoDelay(noDelay) {
  this._deferToConnect('setNoDelay', [noDelay]);
};

ClientRequest.prototype.setSocketKeepAlive =
    function setSocketKeepAlive(enable, initialDelay) {
      this._deferToConnect('setKeepAlive', [enable, initialDelay]);
    };

ClientRequest.prototype.clearTimeout = function clearTimeout(cb) {
  this.setTimeout(0, cb);
};
