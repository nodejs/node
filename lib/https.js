'use strict';

require('internal/util').assertCrypto(exports);

const tls = require('tls');
const url = require('url');
const http = require('http');
const util = require('util');
const inherits = util.inherits;
const debug = util.debuglog('https');

function Server(opts, requestListener) {
  if (!(this instanceof Server)) return new Server(opts, requestListener);

  if (process.features.tls_npn && !opts.NPNProtocols) {
    opts.NPNProtocols = ['http/1.1', 'http/1.0'];
  }

  if (process.features.tls_alpn && !opts.ALPNProtocols) {
    // http/1.0 is not defined as Protocol IDs in IANA
    // http://www.iana.org/assignments/tls-extensiontype-values
    //       /tls-extensiontype-values.xhtml#alpn-protocol-ids
    opts.ALPNProtocols = ['http/1.1'];
  }

  tls.Server.call(this, opts, http._connectionListener);

  this.httpAllowHalfOpen = false;

  if (requestListener) {
    this.addListener('request', requestListener);
  }

  this.addListener('tlsClientError', function(err, conn) {
    if (!this.emit('clientError', err, conn))
      conn.destroy(err);
  });

  this.timeout = 2 * 60 * 1000;
}
inherits(Server, tls.Server);
exports.Server = Server;

Server.prototype.setTimeout = http.Server.prototype.setTimeout;

exports.createServer = function(opts, requestListener) {
  return new Server(opts, requestListener);
};


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
  http.Agent.call(this, options);
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
inherits(Agent, http.Agent);
Agent.prototype.createConnection = createConnection;

Agent.prototype.getName = function(options) {
  var name = http.Agent.prototype.getName.call(this, options);

  name += ':';
  if (options.ca)
    name += options.ca;

  name += ':';
  if (options.cert)
    name += options.cert;

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

exports.globalAgent = globalAgent;
exports.Agent = Agent;

exports.request = function(options, cb) {
  if (typeof options === 'string') {
    options = url.parse(options);
    if (!options.hostname) {
      throw new Error('Unable to determine the domain name');
    }
  } else {
    options = util._extend({}, options);
  }
  options._defaultAgent = globalAgent;
  return http.request(options, cb);
};

exports.get = function(options, cb) {
  var req = exports.request(options, cb);
  req.end();
  return req;
};
