'use strict';

const { WeakReference } = require('internal/util');
const {
  setDeserializeMainFunction
} = require('v8').startupSnapshot

let obj = { hello: 'world' };
const ref = new WeakReference(obj);
let gcCount = 0;
let maxGC = 10;

function run() {
  globalThis.gc();
  setImmediate(() => {
    gcCount++;
    if (ref.get() === undefined) {
      return;
    } else if (gcCount < maxGC) {
      run();
    } else {
      throw new Error(`Reference is still around after ${maxGC} GC`);
    }
  });
}

setDeserializeMainFunction(() => {
  obj = null;
  run();
});
