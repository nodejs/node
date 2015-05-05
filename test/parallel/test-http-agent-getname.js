'use strict';

var assert = require('assert');
var http = require('http');
var common = require('../common');

var agent = new http.Agent();

// default to localhost
assert.equal(
  agent.getName({
    port: 80,
    localAddress: '192.168.1.1'
  }),
  'localhost:80:192.168.1.1'
);

// empty
assert.equal(
  agent.getName({}),
  'localhost::'
);

// pass all arguments
assert.equal(
  agent.getName({
    host: '0.0.0.0',
    port: 80,
    localAddress: '192.168.1.1'
  }),
  '0.0.0.0:80:192.168.1.1'
);
