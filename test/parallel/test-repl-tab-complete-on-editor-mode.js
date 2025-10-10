'use strict';

const common = require('../common');
const assert = require('assert');
const { startNewREPLServer } = require('../common/repl');

// Tab completion in editor mode
{
  const { replServer, input } = startNewREPLServer();

  input.run(['.clear', '.editor']);

  replServer.completer('Uin', common.mustCall((_error, data) => {
    assert.deepStrictEqual(data, [['Uint'], 'Uin']);
  }));

  input.run(['.clear', '.editor']);

  replServer.completer('var log = console.l', common.mustCall((_error, data) => {
    assert.deepStrictEqual(data, [['console.log'], 'console.l']);
  }));
}

// Regression test for https://github.com/nodejs/node/issues/43528
{
  const { replServer } = startNewREPLServer();

  // Editor mode
  replServer.write('.editor\n');

  replServer.write('a');
  replServer.write(null, { name: 'tab' }); // Should not throw

  replServer.close();
}
