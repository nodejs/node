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

const {
  ObjectDefineProperty,
} = primordials;

const httpAgent = require('_http_agent');
const { ClientRequest } = require('_http_client');
const { methods } = require('_http_common');
const { IncomingMessage } = require('_http_incoming');
const { OutgoingMessage } = require('_http_outgoing');
const {
  _connectionListener,
  STATUS_CODES,
  Server,
  ServerResponse
} = require('_http_server');
let maxHeaderSize;

function createServer(opts, requestListener) {
  return new Server(opts, requestListener);
}

function request(url, options, cb) {
  return new ClientRequest(url, options, cb);
}

function get(url, options, cb) {
  const req = request(url, options, cb);
  req.end();
  return req;
}

module.exports = {
  _connectionListener,
  METHODS: methods.slice().sort(),
  STATUS_CODES,
  Agent: httpAgent.Agent,
  ClientRequest,
  IncomingMessage,
  OutgoingMessage,
  Server,
  ServerResponse,
  createServer,
  get,
  request
};

ObjectDefineProperty(module.exports, 'maxHeaderSize', {
  configurable: true,
  enumerable: true,
  get() {
    if (maxHeaderSize === undefined) {
      const { getOptionValue } = require('internal/options');
      maxHeaderSize = getOptionValue('--max-http-header-size');
    }

    return maxHeaderSize;
  }
});

ObjectDefineProperty(module.exports, 'globalAgent', {
  configurable: true,
  enumerable: true,
  get() {
    return httpAgent.globalAgent;
  },
  set(value) {
    httpAgent.globalAgent = value;
  }
});
