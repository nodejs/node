'use strict';
// Copy console accessor because requiring ../common touches it
const consoleDescriptor = Object.getOwnPropertyDescriptor(global, 'console');
Object.defineProperty(global, 'console', {
  configurable: true,
  writable: true,
  value: {},
});

require('../../common');

// This test checks that, if Node cannot put together the `console` object
// because it is low on stack space while doing so, it can succeed later
// once the stack has unwound a little, and `console` is in a usable state then.

let compiledConsole;

function a() {
  try {
    return a();
  } catch (e) {
    compiledConsole = consoleDescriptor.value;
    if (compiledConsole.log) {
      // Using `console.log` itself might not succeed yet, but the code for it
      // has been compiled.
    } else {
      throw e;
    }
  }
}

a();

compiledConsole.log('Hello, World!');
