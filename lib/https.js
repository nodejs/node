'use strict';

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

  tls.Server.call(this, opts, http._connectionListener);

  this.httpAllowHalfOpen = false;

  if (requestListener) {
    this.addListener('request', requestListener);
  }

  this.addListener('clientError', function(err, conn) {
    conn.destroy();
  });

  this.timeout = 2 * 60 * 1000;
}
inherits(Server, tls.Server);
exports.Server = Server;

Server.prototype.setTimeout = http.Server.prototype.setTimeout;

exports.createServer = function(opts, requestListener) {
  return new Server(opts, requestListener);
};


// HTTPS request backwards compatibility
exports.globalAgent = new http.Agent();
exports.Agent = http.Agent;

exports.request = function(options, cb) {
  if (typeof options === 'string') {
    options = url.parse(options);
  } else {
    options = util._extend({}, options);
  }
  options._defaultAgent = exports.globalAgent;
  options.protocol = 'https:';
  return http.request(options, cb);
};

exports.get = function(options, cb) {
  var req = exports.request(options, cb);
  req.end();
  return req;
};
