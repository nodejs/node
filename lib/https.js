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

require('internal/util').assertCrypto();

const tls = require('tls');
const url = require('url');
const util = require('util');
const { Agent: HttpAgent } = require('_http_agent');
const {
  Server: HttpServer,
  _connectionListener
} = require('_http_server');
const { ClientRequest } = require('_http_client');
const { inherits } = util;
const debug = util.debuglog('https');
const { urlToOptions, searchParamsSymbol } = require('internal/url');
const errors = require('internal/errors');

function Server(opts, requestListener) {
  if (!(this instanceof Server)) return new Server(opts, requestListener);

  if (typeof opts === 'function') {
    requestListener = opts;
    opts = undefined;
  }
  opts = util._extend({}, opts);

  if (process.features.tls_npn && !opts.NPNProtocols) {
    opts.NPNProtocols = ['http/1.1', 'http/1.0'];
  }

  if (process.features.tls_alpn && !opts.ALPNProtocols) {
    // http/1.0 is not defined as Protocol IDs in IANA
    // http://www.iana.org/assignments/tls-extensiontype-values
    //       /tls-extensiontype-values.xhtml#alpn-protocol-ids
    opts.ALPNProtocols = ['http/1.1'];
  }

  tls.Server.call(this, opts, _connectionListener);

  this.httpAllowHalfOpen = false;

  if (requestListener) {
    this.addListener('request', requestListener);
  }

  this.addListener('tlsClientError', function addListener(err, conn) {
    if (!this.emit('clientError', err, conn))
      conn.destroy(err);
  });

  this.timeout = 2 * 60 * 1000;
  this.keepAliveTimeout = 5000;
}
inherits(Server, tls.Server);

Server.prototype.setTimeout = HttpServer.prototype.setTimeout;

function createServer(opts, requestListener) {
  return new Server(opts, requestListener);
}


// HTTPS agents.

function createConnection(port, host, options) {
  if (port !== null && typeof port === 'object') {
    options = port;
  } else if (host !== null && typeof host === 'object') {
    options = host;
  } else if (options === null || typeof options !== 'object') {
    options = {};
  }

  if (typeof port === 'number') {
    options.port = port;
  }

  if (typeof host === 'string') {
    options.host = host;
  }

  debug('createConnection', options);

  if (options._agentKey) {
    const session = this._getSession(options._agentKey);
    if (session) {
      debug('reuse session for %j', options._agentKey);
      options = util._extend({
        session: session
      }, options);
    }
  }

  const socket = tls.connect(options, () => {
    if (!options._agentKey)
      return;

    this._cacheSession(options._agentKey, socket.getSession());
  });

  // Evict session on error
  socket.once('close', (err) => {
    if (err)
      this._evictSession(options._agentKey);
  });

  return socket;
}


function Agent(options) {
  if (!(this instanceof Agent))
    return new Agent(options);

  HttpAgent.call(this, options);
  this.defaultPort = 443;
  this.protocol = 'https:';
  this.maxCachedSessions = this.options.maxCachedSessions;
  if (this.maxCachedSessions === undefined)
    this.maxCachedSessions = 100;

  this._sessionCache = {
    map: {},
    list: []
  };
}
inherits(Agent, HttpAgent);
Agent.prototype.createConnection = createConnection;

Agent.prototype.getName = function getName(options) {
  var name = HttpAgent.prototype.getName.call(this, options);

  name += ':';
  if (options.ca)
    name += options.ca;

  name += ':';
  if (options.cert)
    name += options.cert;

  name += ':';
  if (options.clientCertEngine)
    name += options.clientCertEngine;

  name += ':';
  if (options.ciphers)
    name += options.ciphers;

  name += ':';
  if (options.key)
    name += options.key;

  name += ':';
  if (options.pfx)
    name += options.pfx;

  name += ':';
  if (options.rejectUnauthorized !== undefined)
    name += options.rejectUnauthorized;

  name += ':';
  if (options.servername && options.servername !== options.host)
    name += options.servername;

  name += ':';
  if (options.secureProtocol)
    name += options.secureProtocol;

  return name;
};

Agent.prototype._getSession = function _getSession(key) {
  return this._sessionCache.map[key];
};

Agent.prototype._cacheSession = function _cacheSession(key, session) {
  // Cache is disabled
  if (this.maxCachedSessions === 0)
    return;

  // Fast case - update existing entry
  if (this._sessionCache.map[key]) {
    this._sessionCache.map[key] = session;
    return;
  }

  // Put new entry
  if (this._sessionCache.list.length >= this.maxCachedSessions) {
    const oldKey = this._sessionCache.list.shift();
    debug('evicting %j', oldKey);
    delete this._sessionCache.map[oldKey];
  }

  this._sessionCache.list.push(key);
  this._sessionCache.map[key] = session;
};

Agent.prototype._evictSession = function _evictSession(key) {
  const index = this._sessionCache.list.indexOf(key);
  if (index === -1)
    return;

  this._sessionCache.list.splice(index, 1);
  delete this._sessionCache.map[key];
};

const globalAgent = new Agent();

function request(options, cb) {
  if (typeof options === 'string') {
    options = url.parse(options);
    if (!options.hostname) {
      throw new errors.Error('ERR_INVALID_DOMAIN_NAME');
    }
  } else if (options && options[searchParamsSymbol] &&
             options[searchParamsSymbol][searchParamsSymbol]) {
    // url.URL instance
    options = urlToOptions(options);
  } else {
    options = util._extend({}, options);
  }
  options._defaultAgent = globalAgent;
  return new ClientRequest(options, cb);
}

function get(options, cb) {
  const req = request(options, cb);
  req.end();
  return req;
}

module.exports = {
  Agent,
  globalAgent,
  Server,
  createServer,
  get,
  request
};
