'use strict';

const url = require('url');
const util = require('util');
const urlToOptions = require('internal/url').urlToOptions;

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

exports.createServer = function createServer(requestListener) {
  return new Server(requestListener);
};

const client = require('_http_client');
const ClientRequest = exports.ClientRequest = client.ClientRequest;

exports.request = function request(options, cb) {
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
  options._defaultAgent = options._defaultAgent || exports.globalAgent;
  return new ClientRequest(options, cb);
};

exports.get = function get(options, cb) {
  const req = exports.request(options, cb);
  req.end();
  return req;
};
