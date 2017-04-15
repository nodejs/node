'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');

const baseOptions = {
  method: 'GET',
  port: undefined,
  host: common.localhostIPv4,
};

const failingAgentOptions = [
  true,
  'agent',
  {},
  1,
  () => null,
  Symbol(),
];

const acceptableAgentOptions = [
  false,
  undefined,
  null,
  new http.Agent(),
];

const server = http.createServer((req, res) => {
  res.end('hello');
});

let numberOfResponses = 0;

function createRequest(agent) {
  const options = Object.assign(baseOptions, {agent});
  const request = http.request(options);
  request.end();
  request.on('response', common.mustCall(() => {
    numberOfResponses++;
    if (numberOfResponses === acceptableAgentOptions.length) {
      server.close();
    }
  }));
}

server.listen(0, baseOptions.host, common.mustCall(function() {
  baseOptions.port = this.address().port;

  failingAgentOptions.forEach((agent) => {
    assert.throws(
      () => createRequest(agent),
      /^TypeError: Agent option must be an Agent-like object/,
      `Expected typeof agent: ${typeof agent} to be rejected`
    );
  });

  acceptableAgentOptions.forEach((agent) => {
    assert.doesNotThrow(() => createRequest(agent));
  });
}));

process.on('exit', () => {
  assert.strictEqual(numberOfResponses, acceptableAgentOptions.length);
});
