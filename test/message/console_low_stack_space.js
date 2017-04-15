'use strict';
// copy console accessor because requiring ../common touches it
const consoleDescriptor = Object.getOwnPropertyDescriptor(global, 'console');
delete global.console;
global.console = {};

require('../common');

function a() {
  try {
    return a();
  } catch (e) {
    const console = consoleDescriptor.get();
    if (console.log) {
      console.log('Hello, World!');
    } else {
      throw e;
    }
  }
}

a();
