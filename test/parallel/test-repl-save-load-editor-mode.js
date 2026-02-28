'use strict';

require('../common');
const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');
const { startNewREPLServer } = require('../common/repl');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

// Test for saving a REPL session in editor mode

const { replServer, input } = startNewREPLServer();

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
