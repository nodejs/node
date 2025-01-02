'use strict';

require('../common');
const assert = require('assert');
const vm = require('vm');

// Test that creating cached data for an empty function doesn't crash
{
  const script = new vm.Script('');
  // Get cached data from empty script
  const cachedData = script.createCachedData();

  // Previously this would trigger a fatal V8 error:
  // FATAL ERROR: v8::ScriptCompiler::CreateCodeCacheForFunction Expected SharedFunctionInfo with wrapped source code
  vm.compileFunction('', undefined, {
    cachedData,
    produceCachedData: true
  });
}

// Test normal case with actual function content still works
{
  const script = new vm.Script('function test() { return 42; }');
  const cachedData = script.createCachedData();

  const fn = vm.compileFunction('return 42', undefined, {
    cachedData,
    produceCachedData: true
  });

  const expected = 42;
  assert.strictEqual(fn(), expected);
}
