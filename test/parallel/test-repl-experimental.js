// Flags: --experimental-repl --expose-internals

'use strict';

const common = require('../common');
const assert = require('assert');
const repl = require('repl');
const ExperimentalREPL = require('internal/repl/experimental/index');
const ExperimentalREPLUtilities = require('internal/repl/experimental/util');

// UTILITIES
{
  // Test isIdentifier
  assert.strictEqual(ExperimentalREPLUtilities.isIdentifier(''), false);
  assert.strictEqual(ExperimentalREPLUtilities.isIdentifier(9876), false);
  assert.strictEqual(ExperimentalREPLUtilities.isIdentifier('identifier'), true);

  // Test strEscape
  assert.strictEqual(ExperimentalREPLUtilities.strEscape('string'), '\'string\'');
  assert.strictEqual(ExperimentalREPLUtilities.strEscape('str\'i\'ng'), '\'str\\\'i\\\'ng\'');

  // Test getStringWidth
  assert.strictEqual(ExperimentalREPLUtilities.getStringWidth('string'), 6);
  assert.strictEqual(ExperimentalREPLUtilities.getStringWidth('\x1b[4mstring\x1b[24m'), 6);
  assert.strictEqual(ExperimentalREPLUtilities.getStringWidth(String.fromCodePoint(0x20000)), 2);

  // Test stripVTControlCharacters
  assert.strictEqual(ExperimentalREPLUtilities.stripVTControlCharacters('\x1b[4mstring\x1b[24m'), 'string');
  assert.strictEqual(ExperimentalREPLUtilities.stripVTControlCharacters('string'), 'string');

  // Test isStaticIdentifier
  // // VariableDeclaration
  assert.strictEqual(ExperimentalREPLUtilities.isStaticIdentifier('let x = 42'), false);
  assert.strictEqual(ExperimentalREPLUtilities.isStaticIdentifier('const x = 42'), false);
  assert.strictEqual(ExperimentalREPLUtilities.isStaticIdentifier('var x = 42'), false);
  // // FunctionDeclaration
  assert.strictEqual(ExperimentalREPLUtilities.isStaticIdentifier('function runner() {}'), false);
  // // FunctionExpression
  assert.strictEqual(ExperimentalREPLUtilities.isStaticIdentifier('const runner = function() {}'), false);
  // // ArrowFunctionExpression
  assert.strictEqual(ExperimentalREPLUtilities.isStaticIdentifier('const runner = () => {}'), false);
  // // AssignmentExpression
  assert.strictEqual(ExperimentalREPLUtilities.isStaticIdentifier('x = 42'), false);
  // // UpdateExpression
  assert.strictEqual(ExperimentalREPLUtilities.isStaticIdentifier('x++'), false);
  assert.strictEqual(ExperimentalREPLUtilities.isStaticIdentifier('++x'), false);
  assert.strictEqual(ExperimentalREPLUtilities.isStaticIdentifier('x--'), false);
  assert.strictEqual(ExperimentalREPLUtilities.isStaticIdentifier('--x'), false);
  // // NewExpression
  assert.strictEqual(ExperimentalREPLUtilities.isStaticIdentifier('new Date()'), false);
  // // MethodDefinition
  assert.strictEqual(ExperimentalREPLUtilities.isStaticIdentifier('class Runner { method() {} }'), false);
  // // CallExpression
  assert.strictEqual(ExperimentalREPLUtilities.isStaticIdentifier('runner()'), false);
  // // ClassExpression
  assert.strictEqual(ExperimentalREPLUtilities.isStaticIdentifier('const Runner = class {}'), false);
  // // ClassDeclaration
  assert.strictEqual(ExperimentalREPLUtilities.isStaticIdentifier('class Runner {}'), false);
  // // ImportDeclaration
  assert.strictEqual(ExperimentalREPLUtilities.isStaticIdentifier('import { runner } from \'./runner\''), false);
  // // ExportNamedDeclaration
  assert.strictEqual(ExperimentalREPLUtilities.isStaticIdentifier('export const runner = 42'), false);
  // // ExportDefaultDeclaration
  assert.strictEqual(ExperimentalREPLUtilities.isStaticIdentifier('export default 42'), false);
  // // ExportAllDeclaration
  assert.strictEqual(ExperimentalREPLUtilities.isStaticIdentifier('export * from \'./runner\''), false);

  // Test isScopeOpen
  assert.strictEqual(ExperimentalREPLUtilities.isScopeOpen('function runner() {'), true);
  assert.strictEqual(ExperimentalREPLUtilities.isScopeOpen('function runner() {}'), false);
  assert.strictEqual(ExperimentalREPLUtilities.isScopeOpen('1+'), true);
  assert.strictEqual(ExperimentalREPLUtilities.isScopeOpen('1+1'), false);
}

(async () => {
  const experimentalREPL = new repl();
  await experimentalREPL.start();

  // Verify that the exported ExperimentalREPL class is the same
  assert.strictEqual(repl, ExperimentalREPL);

  // Check the evaluation of simple expressions
  assert.strictEqual(experimentalREPL.evaluate('1 + 1'), 2);
  assert.strictEqual(experimentalREPL.evaluate('globalThis.module'), experimentalREPL.context.module);

  experimentalREPL.evaluate('globalThis.object = { key: 42 }');

  // Check line completion
  assert.notStrictEqual(experimentalREPL.complete('object.')?.completions, ['key']);
  assert.notStrictEqual(experimentalREPL.complete('object.ke')?.completions, ['key']);
  assert.notStrictEqual(experimentalREPL.complete('.')?.completions, Array.from(experimentalREPL.replCommands.keys()));

  // Check preview
  assert.strictEqual(experimentalREPL.preview('object'), '{ key: 42 }');

  // Check defining commands
  experimentalREPL.defineCommand(
    'test', // name
    'Test command', // help
    () => 'test' // fn
  );

  assert.strictEqual(experimentalREPL.replCommands.get('test').fn(), 'test');
  assert.strictEqual(experimentalREPL.replCommands.get('test').help, 'Test command');

  try {
    await experimentalREPL.onLine('1+=1');
  } catch (e) {
    assert.strictEqual(e.message, 'Invalid left-hand side in assignment');
  }

  await experimentalREPL.onLine('"hello" + "world"');
  assert.strictEqual(experimentalREPL.context._, 'helloworld');

  experimentalREPL.close();
})().then(common.mustCall());
