'use strict';

const common = require('../common');
const assert = require('assert');
const net = require('net');

process.chdir(common.fixturesDir);
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

var answer = '';
server.listen(options, function() {
  options.port = this.address().port;
  const conn = net.connect(options);
  conn.setEncoding('utf8');
  conn.on('data', (data) => answer += data);
  conn.write('require("baz")\nrequire("./baz")\n.exit\n');
});

process.on('exit', function() {
  assert.strictEqual(false, /Cannot find module/.test(answer));
  assert.strictEqual(false, /Error/.test(answer));
  assert.strictEqual(answer, '\'eye catcher\'\n\'perhaps I work\'\n');
});
