// Flags: --experimental-modules
import '../common';
import assert from 'assert';

async function doTest() {
  await assert.rejects(
    async () => {
      await import('../fixtures/es-module-loaders/throw-undefined');
    },
    (e) => e === undefined
  );
}

doTest();
