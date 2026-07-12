'use strict';

const common = require('../common');
const { startNewREPLServer } = require('../common/repl');
const assert = require('node:assert');
const fs = require('node:fs');
const path = require('node:path');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

// Tests the `.history clear` REPL command, which clears the in-memory history
// and truncates the persisted history file.
// Refs: https://github.com/nodejs/node/issues/63905

// The command clears the in-memory history and resets the navigation index.
{
  const { replServer, input, output } = startNewREPLServer({ terminal: false });

  replServer.history = ['const a = 1', 'a + 1', 'process.version'];
  replServer.historyIndex = 2;

  input.run(['.history clear']);

  assert.strictEqual(replServer.history.length, 0);
  assert.strictEqual(replServer.historyIndex, -1);
  assert.match(output.accumulator, /Clearing history/);

  replServer.close();
}

// A bare `.history` (or an unknown subcommand) prints usage and clears nothing.
{
  const { replServer, input, output } = startNewREPLServer({ terminal: false });

  replServer.history = ['const a = 1'];
  input.run(['.history']);

  assert.match(output.accumulator, /Usage: \.history clear/);
  assert.strictEqual(replServer.history.length, 1);

  replServer.close();
}

// The command truncates the persisted history file.
{
  const filePath = path.resolve(tmpdir.path, '.node_repl_history_clear');
  fs.writeFileSync(filePath, 'const a = 1\na + 1\nprocess.version\n');

  const { replServer, input } = startNewREPLServer({ terminal: false });

  replServer.setupHistory({ filePath }, common.mustSucceed(() => {
    assert.ok(replServer.history.length > 0);
    assert.ok(fs.readFileSync(filePath, 'utf8').length > 0);

    input.run(['.history clear']);
    assert.strictEqual(replServer.history.length, 0);

    // File truncation is asynchronous; await it before reading the file back.
    replServer.historyManager.clearHistory().then(common.mustCall(() => {
      assert.strictEqual(fs.readFileSync(filePath, 'utf8'), '');
      replServer.close();
    }));
  }));
}
