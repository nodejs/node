// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';

const common = require('../common');
const ArrayStream = require('../common/arraystream');
const {
  hijackStderr,
  restoreStderr
} = require('../common/hijackstdio');
const assert = require('assert');
const fixtures = require('../common/fixtures');
const hasInspector = process.features.inspector;

if (!common.isMainThread)
  common.skip('process.chdir is not available in Workers');

// We have to change the directory to ../fixtures before requiring repl
// in order to make the tests for completion of node_modules work properly
// since repl modifies module.paths.
process.chdir(fixtures.fixturesDir);

const repl = require('repl');

function getNoResultsFunction() {
  return common.mustCall((err, data) => {
    assert.ifError(err);
    assert.deepStrictEqual(data[0], []);
  });
}

const works = [['inner.one'], 'inner.o'];
const putIn = new ArrayStream();
const testMe = repl.start('', putIn);

// Some errors are passed to the domain, but do not callback
testMe._domain.on('error', function(err) {
  assert.ifError(err);
});

// Tab Complete will not break in an object literal
putIn.run(['.clear']);
putIn.run([
  'var inner = {',
  'one:1'
]);
testMe.complete('inner.o', getNoResultsFunction());

testMe.complete('console.lo', common.mustCall(function(error, data) {
  assert.deepStrictEqual(data, [['console.log'], 'console.lo']);
}));

// Tab Complete will return globally scoped variables
putIn.run(['};']);
testMe.complete('inner.o', common.mustCall(function(error, data) {
  assert.deepStrictEqual(data, works);
}));

putIn.run(['.clear']);

// Tab Complete will not break in an ternary operator with ()
putIn.run([
  'var inner = ( true ',
  '?',
  '{one: 1} : '
]);
testMe.complete('inner.o', getNoResultsFunction());

putIn.run(['.clear']);

// Tab Complete will return a simple local variable
putIn.run([
  'var top = function() {',
  'var inner = {one:1};'
]);
testMe.complete('inner.o', common.mustCall(function(error, data) {
  assert.deepStrictEqual(data, works);
}));

// When you close the function scope tab complete will not return the
// locally scoped variable
putIn.run(['};']);
testMe.complete('inner.o', getNoResultsFunction());

putIn.run(['.clear']);

// Tab Complete will return a complex local variable
putIn.run([
  'var top = function() {',
  'var inner = {',
  ' one:1',
  '};'
]);
testMe.complete('inner.o', common.mustCall(function(error, data) {
  assert.deepStrictEqual(data, works);
}));

putIn.run(['.clear']);

// Tab Complete will return a complex local variable even if the function
// has parameters
putIn.run([
  'var top = function(one, two) {',
  'var inner = {',
  ' one:1',
  '};'
]);
testMe.complete('inner.o', common.mustCall(function(error, data) {
  assert.deepStrictEqual(data, works);
}));

putIn.run(['.clear']);

// Tab Complete will return a complex local variable even if the
// scope is nested inside an immediately executed function
putIn.run([
  'var top = function() {',
  '(function test () {',
  'var inner = {',
  ' one:1',
  '};'
]);
testMe.complete('inner.o', common.mustCall(function(error, data) {
  assert.deepStrictEqual(data, works);
}));

putIn.run(['.clear']);

// def has the params and { on a separate line
putIn.run([
  'var top = function() {',
  'r = function test (',
  ' one, two) {',
  'var inner = {',
  ' one:1',
  '};'
]);
testMe.complete('inner.o', common.mustCall(function(error, data) {
  assert.deepStrictEqual(data, works);
}));

putIn.run(['.clear']);

// Currently does not work, but should not break, not the {
putIn.run([
  'var top = function() {',
  'r = function test ()',
  '{',
  'var inner = {',
  ' one:1',
  '};'
]);
testMe.complete('inner.o', getNoResultsFunction());

putIn.run(['.clear']);

// currently does not work, but should not break
putIn.run([
  'var top = function() {',
  'r = function test (',
  ')',
  '{',
  'var inner = {',
  ' one:1',
  '};'
]);
testMe.complete('inner.o', getNoResultsFunction());

putIn.run(['.clear']);

// make sure tab completion works on non-Objects
putIn.run([
  'var str = "test";'
]);
testMe.complete('str.len', common.mustCall(function(error, data) {
  assert.deepStrictEqual(data, [['str.length'], 'str.len']);
}));

putIn.run(['.clear']);

// tab completion should not break on spaces
const spaceTimeout = setTimeout(function() {
  throw new Error('timeout');
}, 1000);

testMe.complete(' ', common.mustCall(function(error, data) {
  assert.deepStrictEqual(data, [[], undefined]);
  clearTimeout(spaceTimeout);
}));

// Tab completion should pick up the global "toString" object, and
// any other properties up the "global" object's prototype chain
testMe.complete('toSt', common.mustCall(function(error, data) {
  assert.deepStrictEqual(data, [['toString'], 'toSt']);
}));

// Own properties should shadow properties on the prototype
putIn.run(['.clear']);
putIn.run([
  'var x = Object.create(null);',
  'x.a = 1;',
  'x.b = 2;',
  'var y = Object.create(x);',
  'y.a = 3;',
  'y.c = 4;'
]);
testMe.complete('y.', common.mustCall(function(error, data) {
  assert.deepStrictEqual(data, [['y.b', '', 'y.a', 'y.c'], 'y.']);
}));

// Tab complete provides built in libs for require()
putIn.run(['.clear']);

testMe.complete('require(\'', common.mustCall(function(error, data) {
  assert.strictEqual(error, null);
  repl._builtinLibs.forEach(function(lib) {
    assert(data[0].includes(lib), `${lib} not found`);
  });
}));

testMe.complete('require(\'n', common.mustCall(function(error, data) {
  assert.strictEqual(error, null);
  assert.strictEqual(data.length, 2);
  assert.strictEqual(data[1], 'n');
  assert(data[0].includes('net'));
  // It's possible to pick up non-core modules too
  data[0].forEach(function(completion) {
    if (completion)
      assert(/^n/.test(completion));
  });
}));

{
  const expected = ['@nodejsscope', '@nodejsscope/'];
  putIn.run(['.clear']);
  testMe.complete('require(\'@nodejs', common.mustCall((err, data) => {
    assert.strictEqual(err, null);
    assert.deepStrictEqual(data, [expected, '@nodejs']);
  }));
}

// Test tab completion for require() relative to the current directory
{
  putIn.run(['.clear']);

  const cwd = process.cwd();
  process.chdir(__dirname);

  ['require(\'.', 'require(".'].forEach((input) => {
    testMe.complete(input, common.mustCall((err, data) => {
      assert.strictEqual(err, null);
      assert.strictEqual(data.length, 2);
      assert.strictEqual(data[1], '.');
      assert.strictEqual(data[0].length, 2);
      assert.ok(data[0].includes('./'));
      assert.ok(data[0].includes('../'));
    }));
  });

  ['require(\'..', 'require("..'].forEach((input) => {
    testMe.complete(input, common.mustCall((err, data) => {
      assert.strictEqual(err, null);
      assert.deepStrictEqual(data, [['../'], '..']);
    }));
  });

  ['./', './test-'].forEach((path) => {
    [`require('${path}`, `require("${path}`].forEach((input) => {
      testMe.complete(input, common.mustCall((err, data) => {
        assert.strictEqual(err, null);
        assert.strictEqual(data.length, 2);
        assert.strictEqual(data[1], path);
        assert.ok(data[0].includes('./test-repl-tab-complete'));
      }));
    });
  });

  ['../parallel/', '../parallel/test-'].forEach((path) => {
    [`require('${path}`, `require("${path}`].forEach((input) => {
      testMe.complete(input, common.mustCall((err, data) => {
        assert.strictEqual(err, null);
        assert.strictEqual(data.length, 2);
        assert.strictEqual(data[1], path);
        assert.ok(data[0].includes('../parallel/test-repl-tab-complete'));
      }));
    });
  });

  {
    const path = '../fixtures/repl-folder-extensions/f';
    testMe.complete(`require('${path}`, common.mustCall((err, data) => {
      assert.ifError(err);
      assert.strictEqual(data.length, 2);
      assert.strictEqual(data[1], path);
      assert.ok(data[0].includes('../fixtures/repl-folder-extensions/foo.js'));
    }));
  }

  process.chdir(cwd);
}

// Make sure tab completion works on context properties
putIn.run(['.clear']);

putIn.run([
  'var custom = "test";'
]);
testMe.complete('cus', common.mustCall(function(error, data) {
  assert.deepStrictEqual(data, [['custom'], 'cus']);
}));

// Make sure tab completion doesn't crash REPL with half-baked proxy objects.
// See: https://github.com/nodejs/node/issues/2119
putIn.run(['.clear']);

putIn.run([
  'var proxy = new Proxy({}, {ownKeys: () => { throw new Error(); }});'
]);

testMe.complete('proxy.', common.mustCall(function(error, data) {
  assert.strictEqual(error, null);
  assert(Array.isArray(data));
}));

// Make sure tab completion does not include integer members of an Array
putIn.run(['.clear']);

putIn.run(['var ary = [1,2,3];']);
testMe.complete('ary.', common.mustCall(function(error, data) {
  assert.strictEqual(data[0].includes('ary.0'), false);
  assert.strictEqual(data[0].includes('ary.1'), false);
  assert.strictEqual(data[0].includes('ary.2'), false);
}));

// Make sure tab completion does not include integer keys in an object
putIn.run(['.clear']);
putIn.run(['var obj = {1:"a","1a":"b",a:"b"};']);

testMe.complete('obj.', common.mustCall(function(error, data) {
  assert.strictEqual(data[0].includes('obj.1'), false);
  assert.strictEqual(data[0].includes('obj.1a'), false);
  assert(data[0].includes('obj.a'));
}));

// Don't try to complete results of non-simple expressions
putIn.run(['.clear']);
putIn.run(['function a() {}']);

testMe.complete('a().b.', getNoResultsFunction());

// Works when prefixed with spaces
putIn.run(['.clear']);
putIn.run(['var obj = {1:"a","1a":"b",a:"b"};']);

testMe.complete(' obj.', common.mustCall((error, data) => {
  assert.strictEqual(data[0].includes('obj.1'), false);
  assert.strictEqual(data[0].includes('obj.1a'), false);
  assert(data[0].includes('obj.a'));
}));

// Works inside assignments
putIn.run(['.clear']);

testMe.complete('var log = console.lo', common.mustCall((error, data) => {
  assert.deepStrictEqual(data, [['console.log'], 'console.lo']);
}));

// tab completion for defined commands
putIn.run(['.clear']);

testMe.complete('.b', common.mustCall((error, data) => {
  assert.deepStrictEqual(data, [['break'], 'b']);
}));
putIn.run(['.clear']);
putIn.run(['var obj = {"hello, world!": "some string", "key": 123}']);
testMe.complete('obj.', common.mustCall((error, data) => {
  assert.strictEqual(data[0].includes('obj.hello, world!'), false);
  assert(data[0].includes('obj.key'));
}));

[
  Array,
  Buffer,

  Uint8Array,
  Uint16Array,
  Uint32Array,

  Uint8ClampedArray,
  Int8Array,
  Int16Array,
  Int32Array,
  Float32Array,
  Float64Array,
].forEach((type) => {
  putIn.run(['.clear']);

  if (type === Array) {
    putIn.run([
      'var ele = [];',
      'for (let i = 0; i < 1e6 + 1; i++) ele[i] = 0;',
      'ele.biu = 1;'
    ]);
  } else if (type === Buffer) {
    putIn.run(['var ele = Buffer.alloc(1e6 + 1); ele.biu = 1;']);
  } else {
    putIn.run([`var ele = new ${type.name}(1e6 + 1); ele.biu = 1;`]);
  }

  hijackStderr(common.mustNotCall());
  testMe.complete('ele.', common.mustCall((err, data) => {
    restoreStderr();
    assert.ifError(err);

    const ele = (type === Array) ?
      [] :
      (type === Buffer ?
        Buffer.alloc(0) :
        new type(0));

    assert.strictEqual(data[0].includes('ele.biu'), true);

    data[0].forEach((key) => {
      if (!key || key === 'ele.biu') return;
      assert.notStrictEqual(ele[key.substr(4)], undefined);
    });
  }));
});

// check Buffer.prototype.length not crashing.
// Refs: https://github.com/nodejs/node/pull/11961
putIn.run['.clear'];
testMe.complete('Buffer.prototype.', common.mustCall());

const testNonGlobal = repl.start({
  input: putIn,
  output: putIn,
  useGlobal: false
});

const builtins = [['Infinity', 'Int16Array', 'Int32Array',
                   'Int8Array'], 'I'];

if (common.hasIntl) {
  builtins[0].push('Intl');
}
testNonGlobal.complete('I', common.mustCall((error, data) => {
  assert.deepStrictEqual(data, builtins);
}));

// To test custom completer function.
// Sync mode.
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
    ''
  ]);
}));

// On `a` should output `aaa aa1 aa2` and complete until `aa`.
testCustomCompleterSyncMode.complete('a', common.mustCall((error, data) => {
  assert.deepStrictEqual(data, [
    'aaa aa1 aa2'.split(' '),
    'a'
  ]);
}));

// To test custom completer function.
// Async mode.
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
    ''
  ]);
}));

// On `a` should output `aaa aa1 aa2` and complete until `aa`.
testCustomCompleterAsyncMode.complete('a', common.mustCall((error, data) => {
  assert.deepStrictEqual(data, [
    'aaa aa1 aa2'.split(' '),
    'a'
  ]);
}));

// tab completion in editor mode
const editorStream = new ArrayStream();
const editor = repl.start({
  stream: editorStream,
  terminal: true,
  useColors: false
});

editorStream.run(['.clear']);
editorStream.run(['.editor']);

editor.completer('Uin', common.mustCall((error, data) => {
  assert.deepStrictEqual(data, [['Uint'], 'Uin']);
}));

editorStream.run(['.clear']);
editorStream.run(['.editor']);

editor.completer('var log = console.l', common.mustCall((error, data) => {
  assert.deepStrictEqual(data, [['console.log'], 'console.l']);
}));

{
  // tab completion of lexically scoped variables
  const stream = new ArrayStream();
  const testRepl = repl.start({ stream });

  stream.run([`
    let lexicalLet = true;
    const lexicalConst = true;
    class lexicalKlass {}
  `]);

  ['Let', 'Const', 'Klass'].forEach((type) => {
    const query = `lexical${type[0]}`;
    const expected = hasInspector ? [[`lexical${type}`], query] :
      [[], `lexical${type[0]}`];
    testRepl.complete(query, common.mustCall((error, data) => {
      assert.deepStrictEqual(data, expected);
    }));
  });
}
