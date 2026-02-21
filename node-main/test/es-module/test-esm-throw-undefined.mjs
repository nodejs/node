import '../common/index.mjs';
import assert from 'assert';

async function doTest() {
  await assert.rejects(
    async () => {
      await import('../fixtures/es-module-loaders/throw-undefined.mjs');
    },
    (e) => e === undefined
  );
}

doTest();
