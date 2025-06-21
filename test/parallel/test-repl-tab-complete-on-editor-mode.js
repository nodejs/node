'use strict';

const common = require('../common');
const assert = require('assert');
const ArrayStream = require('../common/arraystream');
const repl = require('repl');

// Tab completion in editor mode
{
  const editorStream = new ArrayStream();
  const editor = repl.start({
    stream: editorStream,
    terminal: true,
    useColors: false
  });

  editorStream.run(['.clear']);
  editorStream.run(['.editor']);

  editor.completer('Uin', common.mustCall((_error, data) => {
    assert.deepStrictEqual(data, [['Uint'], 'Uin']);
  }));

  editorStream.run(['.clear']);
  editorStream.run(['.editor']);

  editor.completer('var log = console.l', common.mustCall((_error, data) => {
    assert.deepStrictEqual(data, [['console.log'], 'console.l']);
  }));
}

// Regression test for https://github.com/nodejs/node/issues/43528
{
  const stream = new ArrayStream();
  const replServer = repl.start({
    input: stream,
    output: stream,
    terminal: true,
  });

  // Editor mode
  replServer.write('.editor\n');

  replServer.write('a');
  replServer.write(null, { name: 'tab' }); // Should not throw

  replServer.close();
}
