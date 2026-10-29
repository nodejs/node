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

  // Tab completion is asynchronous (it is driven by the inspector), so the
  // REPL must only be closed once the completion has settled. Closing it while
  // a completion is still in flight would make the completion callback resume a
  // closed readline interface and throw `ERR_USE_AFTER_CLOSE`.
  const originalCompleter = replServer.completer;
  replServer.completer = common.mustCall((text, cb) => {
    originalCompleter.call(replServer, text, common.mustCall((...args) => {
      const result = cb(...args);
      replServer.close();
      return result;
    }));
  });

  replServer.write(null, { name: 'tab' }); // Should not throw
}
