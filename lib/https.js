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

var tls = require('tls');
var http = require('http');
var util = require('util');
var inherits = require('util').inherits;
var debug = util.debuglog('https');

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


// HTTPS agents.

function createConnection(port, host, options) {
  if (util.isObject(port)) {
    options = port;
  } else if (util.isObject(host)) {
    options = host;
  } else if (util.isObject(options)) {
    options = options;
  } else {
    options = {};
  }

  if (util.isNumber(port)) {
    options.port = port;
  }

  if (util.isString(host)) {
    options.host = host;
  }

  debug('createConnection', options);
  return tls.connect(options);
}


function Agent(options) {
  http.Agent.call(this, options);
  this.defaultPort = 443;
  this.protocol = 'https:';
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
  if (!util.isUndefined(options.rejectUnauthorized))
    name += options.rejectUnauthorized;

  return name;
};

var globalAgent = new Agent();

exports.globalAgent = globalAgent;
exports.Agent = Agent;

exports.request = function(options, cb) {
  return globalAgent.request(options, cb);
};

exports.get = function(options, cb) {
  return globalAgent.get(options, cb);
};
