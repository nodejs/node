'use strict';

const common = require('../common');
const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');
const { startNewREPLServer } = require('../common/repl');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

// Test for saving a REPL session in editor mode

const { replServer, input, run, waitForIdle } = startNewREPLServer();

const commands = [
  'function testSave() {',
  'return "saved";',
  '}',
];

const filePath = path.resolve(tmpdir.path, 'test.save.js');

async function main() {
  await run(['.editor']);

  // Editor-mode lines are buffered synchronously; the evaluation is triggered
  // only when editor mode ends (Ctrl+D), and that evaluation is asynchronous.
  input.run(commands);
  replServer.write('', { ctrl: true, name: 'd' });
  await waitForIdle();

  await run([`.save ${filePath}`]);

  assert.strictEqual(fs.readFileSync(filePath, 'utf8'),
                     `${commands.join('\n')}\n`);

  replServer.close();
}

main().then(common.mustCall());
