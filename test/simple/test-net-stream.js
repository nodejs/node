var common = require('../common');
var assert = require('assert');

var Stream = require('net').Stream;

var s = new Stream();

// test that destroy called on a stream with a server only ever decrements the
// server connection count once

s.server = { connections: 10 }
assert.equal(10, s.server.connections);
s.destroy()
assert.equal(9, s.server.connections);
s.destroy()
assert.equal(9, s.server.connections);
