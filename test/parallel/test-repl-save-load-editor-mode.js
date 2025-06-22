'use strict';

require('../common');
const ArrayStream = require('../common/arraystream');

const assert = require('node:assert');
const fs = require('node:fs');
const repl = require('node:repl');
const path = require('node:path');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

// Test for saving a REPL session in editor mode

const input = new ArrayStream();

const replServer = repl.start({
  prompt: '',
  input,
  output: new ArrayStream(),
  allowBlockingCompletions: true,
  terminal: true,
});

// Some errors are passed to the domain, but do not callback
replServer._domain.on('error', assert.ifError);

input.run(['.editor']);

const commands = [
  'function testSave() {',
  'return "saved";',
  '}',
];

input.run(commands);

replServer.write('', { ctrl: true, name: 'd' });

const filePath = path.resolve(tmpdir.path, 'test.save.js');

input.run([`.save ${filePath}`]);

assert.strictEqual(fs.readFileSync(filePath, 'utf8'),
                   `${commands.join('\n')}\n`);

replServer.close();
