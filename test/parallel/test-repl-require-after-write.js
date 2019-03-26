'use strict';

const common = require('../common');
const assert = require('assert');

const spawn = require('child_process').spawn;
// Use -i to force node into interactive mode, despite stdout not being a TTY
const child = spawn(process.execPath, ['-i']);

let out = '';
const input = "try { require('./non-existent.json'); } catch {} " +
              "require('fs').writeFileSync('./non-existent.json', '1');" +
              "require('./non-existent.json');";

child.stderr.on('data', common.mustNotCall());

child.stdout.setEncoding('utf8');
child.stdout.on('data', (c) => {
  out += c;
});
child.stdout.on('end', common.mustCall(() => {
  assert.strictEqual(out, '> 1\n> ');
}));

child.stdin.end(input);
