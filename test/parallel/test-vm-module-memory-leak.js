'use strict';
// Flags: --experimental-vm-modules --expose-gc

require('../common');
const assert = require('assert');
const { SourceTextModule } = require('vm');
const v8 = require('v8');

async function run() {
  let initialMemory = 0;

  // Run a few times to warm up the VM
  for (let i = 0; i < 5; i++) {
    const m = new SourceTextModule('import.meta.url; const x = new Array(1000).fill(1);', { identifier: `file://warmup${i}.js` });
    await m.link(() => {});
    await m.evaluate();
  }
  global.gc();
  initialMemory = process.memoryUsage().heapUsed;

  process.on('unhandledRejection', () => {});
  const ctx = require('vm').createContext({});
  for (let i = 0; i < 1000; i++) {
    const m = new SourceTextModule(`
      import.meta.url;
      const x = new Array(1024 * 1024).fill(1); // some memory
      await new Promise(r => setTimeout(r, 0));
      throw new Error('foo');
    `, { 
      context: ctx,
      identifier: `file://test${i}.js`,
      initializeImportMeta(meta) { meta.url = `file://test${i}.js`; }
    });
    await m.link(() => {});
    m.evaluate(); // Don't catch!
  }
  
  // Wait for all microtasks to run
  await new Promise(r => setImmediate(r));
  
  delete globalThis.importMetaUrl;
  global.gc();
  
  const finalMemory = process.memoryUsage().heapUsed;
  const diffMB = (finalMemory - initialMemory) / 1024 / 1024;
  
  // The leak was ~80MB for 1000 iterations.
  // We expect the diff to be small (e.g. < 5MB).
  assert(diffMB < 10, `Memory leaked: ${diffMB.toFixed(2)}MB`);
}

run().then(() => {
  console.log('Test passed');
});
