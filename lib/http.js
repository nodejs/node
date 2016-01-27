'use strict';

const util = require('util');
const internalUtil = require('internal/util');
const EventEmitter = require('events');


exports.IncomingMessage = require('_http_incoming').IncomingMessage;


const common = require('_http_common');
exports.METHODS = common.methods.slice().sort();


exports.OutgoingMessage = require('_http_outgoing').OutgoingMessage;


const server = require('_http_server');
exports.ServerResponse = server.ServerResponse;
exports.STATUS_CODES = server.STATUS_CODES;


const agent = require('_http_agent');
const Agent = exports.Agent = agent.Agent;
exports.globalAgent = agent.globalAgent;

const client = require('_http_client');
const ClientRequest = exports.ClientRequest = client.ClientRequest;

exports.request = function(options, cb) {
  return new ClientRequest(options, cb);
};

exports.get = function(options, cb) {
  var req = exports.request(options, cb);
  req.end();
  return req;
};

exports._connectionListener = server._connectionListener;
const Server = exports.Server = server.Server;

exports.createServer = function(requestListener) {
  return new Server(requestListener);
};


// Legacy Interface

function Client(port, host) {
  if (!(this instanceof Client)) return new Client(port, host);
  EventEmitter.call(this);

  host = host || 'localhost';
  port = port || 80;
  this.host = host;
  this.port = port;
  this.agent = new Agent({ host: host, port: port, maxSockets: 1 });
}
util.inherits(Client, EventEmitter);
Client.prototype.request = function(method, path, headers) {

  var options = {};
  options.host = this.host;
  options.port = this.port;
  if (method[0] === '/') {
    headers = path;
    path = method;
    method = 'GET';
  }
  options.method = method;
  options.path = path;
  options.headers = headers;
  options.agent = this.agent;
  var c = new ClientRequest(options);
  c.on('error', (e) => {
    this.emit('error', e);
  });
  // The old Client interface emitted 'end' on socket end.
  // This doesn't map to how we want things to operate in the future
  // but it will get removed when we remove this legacy interface.
  c.on('socket', (s) => {
    s.on('end', () => {
      if (this._decoder) {
        var ret = this._decoder.end();
        if (ret)
          this.emit('data', ret);
      }
      this.emit('end');
    });
  });
  return c;
};

exports.Client = internalUtil.deprecate(Client, 'http.Client is deprecated.');

exports.createClient = internalUtil.deprecate(function(port, host) {
  return new Client(port, host);
}, 'http.createClient is deprecated. Use http.request instead.');
