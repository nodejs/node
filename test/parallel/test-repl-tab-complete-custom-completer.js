'use strict';

const common = require('../common');
const ArrayStream = require('../common/arraystream');
const assert = require('assert');

const repl = require('repl');

const putIn = new ArrayStream();

// To test custom completer function.
// Sync mode.
{
  const customCompletions = 'aaa aa1 aa2 bbb bb1 bb2 bb3 ccc ddd eee'.split(' ');
  const testCustomCompleterSyncMode = repl.start({
    prompt: '',
    input: putIn,
    output: putIn,
    completer: function completer(line) {
      const hits = customCompletions.filter((c) => c.startsWith(line));
      // Show all completions if none found.
      return [hits.length ? hits : customCompletions, line];
    }
  });

  // On empty line should output all the custom completions
  // without complete anything.
  testCustomCompleterSyncMode.complete('', common.mustCall((error, data) => {
    assert.deepStrictEqual(data, [
      customCompletions,
      '',
    ]);
  }));

  // On `a` should output `aaa aa1 aa2` and complete until `aa`.
  testCustomCompleterSyncMode.complete('a', common.mustCall((error, data) => {
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
  const testCustomCompleterAsyncMode = repl.start({
    prompt: '',
    input: putIn,
    output: putIn,
    completer: function completer(line, callback) {
      const hits = customCompletions.filter((c) => c.startsWith(line));
      // Show all completions if none found.
      callback(null, [hits.length ? hits : customCompletions, line]);
    }
  });

  // On empty line should output all the custom completions
  // without complete anything.
  testCustomCompleterAsyncMode.complete('', common.mustCall((error, data) => {
    assert.deepStrictEqual(data, [
      customCompletions,
      '',
    ]);
  }));

  // On `a` should output `aaa aa1 aa2` and complete until `aa`.
  testCustomCompleterAsyncMode.complete('a', common.mustCall((error, data) => {
    assert.deepStrictEqual(data, [
      'aaa aa1 aa2'.split(' '),
      'a',
    ]);
  }));
}
