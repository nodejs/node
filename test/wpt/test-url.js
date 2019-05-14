'use strict';

// Flags: --expose-internals

require('../common');
const assert = require('assert');
const { WPTRunner } = require('../common/wpt');

const runner = new WPTRunner('url');

// Copy global descriptors from the global object
runner.copyGlobalsFromObject(global, ['URL', 'URLSearchParams']);
// Needed by urlsearchparams-constructor.any.js
let DOMException;
runner.defineGlobal('DOMException', {
  get() {
    // A 'hack' to get the DOMException constructor since we don't have it
    // on the global object.
    if (DOMException === undefined) {
      const port = new (require('worker_threads').MessagePort)();
      const ab = new ArrayBuffer(1);
      try {
        port.postMessage(ab, [ab, ab]);
      } catch (err) {
        DOMException = err.constructor;
      }
      assert.strictEqual(DOMException.name, 'DOMException');
    }
    return DOMException;
  }
});

runner.runJsTests();
