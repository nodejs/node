'use strict';
const common = require('../common');
const assert = require('assert');

const spawn = require('child_process').spawn;

const env = {
  'HELLO': 'WORLD'
};
Object.setPrototypeOf(env, {
  'FOO': 'BAR'
});

let child;
if (common.isWindows) {
  child = spawn('cmd.exe', ['/c', 'set'],
                Object.assign({}, process.env, { env: env }));
} else {
  child = spawn('/usr/bin/env', [], { env: env });
  child = spawn('/usr/bin/env', [],
                Object.assign({}, process.env, { env: env }));
}


let response = '';

child.stdout.setEncoding('utf8');

child.stdout.on('data', function(chunk) {
  console.log(`stdout: ${chunk}`);
  response += chunk;
});

process.on('exit', function() {
  assert.ok(response.includes('HELLO=WORLD'));
  assert.ok(response.includes('FOO=BAR'));
});
