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
  return new ClientRequest(options, cb);
};

exports.get = function get(options, cb) {
  var req = exports.request(options, cb);
  req.end();
  return req;
};
