'use strict';
// Flags: --expose-internals

const common = require('../common');
const fs = require('fs');
const path = require('path');
const { ReplHistory } = require('internal/repl/history');
const assert = require('assert');

const tmpdir = require('../common/tmpdir');
tmpdir.refresh();

const historyPath = path.join(tmpdir.path, '.node_repl_history');

fs.writeFileSync(historyPath, 'dummy\n');

const originalOpen = fs.promises.open;
let closeCalled = false;

fs.promises.open = async (filepath, flags, mode) => {
  const handle = await originalOpen(filepath, flags, mode);

  if (flags === 'r+' && filepath === historyPath) {
    handle.truncate = async (len) => {
      throw new Error('Mock truncate error');
    };

    const originalClose = handle.close;
    handle.close = async () => {
      closeCalled = true;
      return originalClose.call(handle);
    };
  }

  return handle;
};

const context = {
  historySize: 30,
  on: () => {},
  once: () => {},
  emit: () => {},
  pause: () => {},
  resume: () => {},
  off: () => {},
  line: '',
  _historyPrev: () => {},
  _writeToOutput: () => {}
};

const history = new ReplHistory(context, { filePath: historyPath });

history.initialize(common.mustCall((err) => {
  assert.strictEqual(err, null);
  assert.strictEqual(closeCalled, true);
}));
