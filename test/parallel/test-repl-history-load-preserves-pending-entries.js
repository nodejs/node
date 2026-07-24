'use strict';

// Lines can be evaluated while the history file is still being loaded
// asynchronously by `setupHistory()`, e.g. when the input stream does not
// support pausing. Entries added to the in-memory history in the meantime
// must not be discarded once the file load completes.
// Refs: https://github.com/nodejs/node/issues/64508

const common = require('../common');
const assert = require('assert');
const fs = require('fs');
const stream = require('stream');
const repl = require('repl');

if (process.env.TERM === 'dumb') {
  common.skip('skipping - dumb terminal');
}

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const historyPath = tmpdir.resolve('.repl_history');
fs.writeFileSync(historyPath, 'persisted entry');

// An input stream that, unlike a TTY, does not buffer data while paused.
class FakeInput extends stream.Stream {
  resume() {}
  pause() {}
}
FakeInput.prototype.readable = true;

const input = new FakeInput();
const output = new stream.Writable({
  write(chunk, encoding, callback) {
    callback();
  },
});

const r = repl.start({
  input,
  output,
  prompt: '',
  terminal: true,
  useColors: false,
});

r.setupHistory(historyPath, common.mustSucceed(() => {
  // The lines evaluated while the history file was being read must be kept,
  // newest first, followed by the persisted entries.
  assert.deepStrictEqual(
    r.history,
    ['const b = 2', 'const a = 1', 'persisted entry'],
  );
  assert.strictEqual(
    fs.readFileSync(historyPath, 'utf8'),
    'const b = 2\nconst a = 1\npersisted entry',
  );
  r.close();
}));

// Evaluated synchronously, before the history file has been read.
input.emit('data', 'const a = 1\nconst b = 2\n');
