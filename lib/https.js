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
var inherits = require('util').inherits;

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
}
inherits(Server, tls.Server);


exports.Server = Server;


exports.createServer = function(opts, requestListener) {
  return new Server(opts, requestListener);
};


// HTTPS agents.

function createConnection(port, host, options) {
  return tls.connect(port, host, options);
};

function Agent(options) {
  http.Agent.call(this, options);
  this.createConnection = createConnection;
};
inherits(Agent, http.Agent);
Agent.prototype.defaultPort = 443;

var globalAgent = new Agent();

exports.globalAgent = globalAgent;
exports.Agent = Agent;

exports.request = function(options, cb) {
  if (options.protocol && options.protocol !== 'https:') {
    throw new Error('Protocol:' + options.protocol + ' not supported.');
  }

  if (options.agent === undefined) {
    options.agent = globalAgent;
  }
  options.createConnection = createConnection;
  options.defaultPort = options.defaultPort || 443;
  return new http.ClientRequest(options, cb);
};

exports.get = function(options, cb) {
  options.method = 'GET';
  var req = exports.request(options, cb);
  req.end();
  return req;
};
