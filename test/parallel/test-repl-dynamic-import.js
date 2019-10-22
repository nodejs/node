'use strict';

// Flags: --experimental-modules

const common = require('../common');
const fixtures = require('../common/fixtures');
const assert = require('assert');
const net = require('net');

if (!common.isMainThread)
  common.skip('process.chdir is not available in Workers');

process.chdir(fixtures.fixturesDir);
const repl = require('repl');

const server = net.createServer((conn) => {
  repl.start('', conn).on('exit', () => {
    conn.destroy();
    server.close();
  });
});

const host = common.localhostIPv4;
const port = 0;
const options = { host, port };

let answer = '';
let expectedDataEvents = 2;
server.listen(options, function() {
  options.port = this.address().port;
  const conn = net.connect(options);
  conn.setEncoding('utf8');
  conn.on('data', (data) => {
    answer += data;
    if (--expectedDataEvents === 0) {
      conn.write('.exit\n');
    }
  });
  conn.write('import("./es-modules/message.mjs").then(console.log)\n');
});

process.on('exit', function() {
  assert.strictEqual(/Cannot find module/.test(answer), false);
  assert.strictEqual(/Error/.test(answer), false);
  assert.strictEqual(/{ message: 'A message' }/.test(answer), true);
});
