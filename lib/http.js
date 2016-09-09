'use strict';
exports.IncomingMessage = require('_http_incoming').IncomingMessage;

exports.OutgoingMessage = require('_http_outgoing').OutgoingMessage;

exports.METHODS = require('_http_common').methods.slice().sort();

const agent = require('_http_agent');
exports.Agent = agent.Agent;
exports.globalAgent = agent.globalAgent;

const server = require('_http_server');
exports.ServerResponse = server.ServerResponse;
exports.STATUS_CODES = server.STATUS_CODES;
exports._connectionListener = server._connectionListener;
const Server = exports.Server = server.Server;

exports.createServer = function(requestListener) {
  return new Server(requestListener);
};

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
