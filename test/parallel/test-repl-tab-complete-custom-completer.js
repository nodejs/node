'use strict';

const common = require('../common');
const assert = require('assert');
const { startNewREPLServer } = require('../common/repl');

// To test custom completer function.
// Sync mode.
{
  const customCompletions = 'aaa aa1 aa2 bbb bb1 bb2 bb3 ccc ddd eee'.split(' ');
  const { replServer } = startNewREPLServer({
    completer: function completer(line) {
      const hits = customCompletions.filter((c) => c.startsWith(line));
      // Show all completions if none found.
      return [hits.length ? hits : customCompletions, line];
    }
  });

  // On empty line should output all the custom completions
  // without complete anything.
  replServer.complete('', common.mustCall((error, data) => {
    assert.deepStrictEqual(data, [
      customCompletions,
      '',
    ]);
  }));

  // On `a` should output `aaa aa1 aa2` and complete until `aa`.
  replServer.complete('a', common.mustCall((error, data) => {
    assert.deepStrictEqual(data, [
      'aaa aa1 aa2'.split(' '),
      'a',
    ]);
  }));
}

// To test custom completer function.
// Async mode.
{
  const customCompletions = 'aaa aa1 aa2 bbb bb1 bb2 bb3 ccc ddd eee'.split(' ');
  const { replServer } = startNewREPLServer({
    completer: function completer(line, callback) {
      const hits = customCompletions.filter((c) => c.startsWith(line));
      // Show all completions if none found.
      callback(null, [hits.length ? hits : customCompletions, line]);
    }
  });

  // On empty line should output all the custom completions
  // without complete anything.
  replServer.complete('', common.mustCall((error, data) => {
    assert.deepStrictEqual(data, [
      customCompletions,
      '',
    ]);
  }));

  // On `a` should output `aaa aa1 aa2` and complete until `aa`.
  replServer.complete('a', common.mustCall((error, data) => {
    assert.deepStrictEqual(data, [
      'aaa aa1 aa2'.split(' '),
      'a',
    ]);
  }));
}
