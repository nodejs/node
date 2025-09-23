'use strict';
// Flags: --expose-internals

// Make sure http.request() can catch immediate errors in
// net.createConnection().

const common = require('../common');
console.log('require completed: common', Date.now());
const assert = require('assert');
console.log('require completed: assert', Date.now());
const net = require('net');
console.log('require completed: net', Date.now());
const http = require('http');
console.log('require completed: http', Date.now());
const { internalBinding } = require('internal/test/binding');
console.log('require completed: internal/test/binding', Date.now());
const { UV_ENETUNREACH, UV_EADDRINUSE } = internalBinding('uv');
const {
  newAsyncId,
  symbols: { async_id_symbol }
} = require('internal/async_hooks');
console.log('require completed: internal/async_hooks', Date.now());

const config = {
  host: 'http://example.com',
  // We need to specify 'family' in both items of the array so 'internalConnectMultiple' is invoked
  connectMultiple: [{ address: '192.4.4.4', family: 4 }, { address: '200::1', family: 6 }],
  connectSolo: { address: '192.4.4.4', family: 4 },
};

function agentFactory(handle, count) {
  const agent = new http.Agent();
  agent.createConnection = common.mustCall((...cfg) => {
    const normalized = net._normalizeArgs(cfg);
    const sock = new net.Socket();

    // Fake the handle so we can enforce returning an immediate error
    sock._handle = handle;

    // Simulate just enough socket handle initialization
    sock[async_id_symbol] = newAsyncId();

    sock.connect(normalized);
    return sock;
  }, count);

  return agent;
};

const handleWithoutBind = callCount => ({
  connect: common.mustCall((req, addr, port) => {
    return UV_ENETUNREACH;
  }, callCount), 
  readStart() { },
  close() { },
});

const handleWithBind = callCount => ({
  readStart() { },
  close() { },
  bind: common.mustCall(() => UV_EADDRINUSE, callCount), 
});

const agentWithoutBind = agentFactory(handleWithoutBind(3), 3); // Because three tests will use this
const agentWithBind = agentFactory(handleWithBind(2), 2); // Because two tests will use this handle

console.log('test initiated: test-1', Date.now());
http.get({
  host: '127.0.0.1',
  port: 1,
  agent: agentWithoutBind,
}).on('error', common.mustCall((err) => {
  assert.strictEqual(err.code, 'ENETUNREACH');
  console.log('test completed: test-1', Date.now());
}));

console.log('test initiated: test-2', Date.now());
http.request(config.host, {
  agent: agentWithoutBind,
  lookup(_0, _1, cb) {
    cb(null, config.connectMultiple);
  },
}).on('error', common.mustCall((err) => {
  assert.strictEqual(err.code, 'ENETUNREACH');
  console.log('test completed: test-2', Date.now());
}));

console.log('test initiated: test-3', Date.now());
http.request(config.host, {
  agent: agentWithoutBind,
  lookup(_0, _1, cb) {
    cb(null, config.connectSolo.address, config.connectSolo.family);
  },
  family: 4,
}).on('error', common.mustCall((err) => {
  assert.strictEqual(err.code, 'ENETUNREACH');
  console.log('test completed: test-3', Date.now());
}));

console.log('test initiated: test-4', Date.now());
http.request(config.host, {
  agent: agentWithBind,
  family: 4, // We specify 'family' so 'internalConnect' is invoked
  localPort: 2222 // Required to trigger _handle.bind()
}).on('error', common.mustCall((err) => {
  assert.strictEqual(err.code, 'EADDRINUSE');
  console.log('test completed: test-4', Date.now());
}));

console.log('test initiated: test-5', Date.now());
http.request(config.host, {
  agent: agentWithBind,
  lookup(_0, _1, cb) {
    cb(null, config.connectMultiple);
  },
  localPort: 2222, // Required to trigger _handle.bind()
}).on('error', common.mustCall((err) => {
  assert.strictEqual(err.code, 'EADDRINUSE');
  console.log('test completed: test-5', Date.now());
}));
