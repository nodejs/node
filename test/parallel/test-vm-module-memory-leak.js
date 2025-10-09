// Flags: --experimental-vm-modules --max-old-space-size=16
'use strict';

// Test for memory leak when creating SourceTextModule instances without
// linking or evaluating them
// Refs: https://github.com/nodejs/node/issues/59118

const common = require('../common');
const { checkIfCollectable } = require('../common/gc');
const vm = require('vm');

// This test verifies that SourceTextModule instances can be properly
// garbage collected even when not linked or evaluated.
//
// Root cause: module_.SetWeak() created an undetectable GC cycle between
// the JavaScript wrapper object and the v8::Module. When both references
// in a cycle are made weak independently, V8's GC cannot detect the cycle.
//
// The fix removes module_.SetWeak(), keeping module_ as a strong reference.
// The module is released when ~ModuleWrap() is called after the wrapper
// object is garbage collected.

checkIfCollectable(() => {
  // Create modules without keeping references
  // These should be collectible after going out of scope
  const context = vm.createContext();
  return new vm.SourceTextModule(`
    const data = new Array(128).fill("test");
    export default data;
  `, {
    context,
  });
  // Module goes out of scope, should be collectible
});
