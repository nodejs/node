'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const assert = require('assert');
const inspector = require('inspector/promises');

const { basename } = require('path');
const currentFilename = basename(__filename);

{
  // Ensure that inspector/promises has the same signature as inspector
  assert.deepStrictEqual(Reflect.ownKeys(inspector), Reflect.ownKeys(require('inspector')));
}

(async () => {
  {
    // Ensure that session.post returns a valid promisified result
    const session = new inspector.Session();
    session.connect();

    await session.post('Profiler.enable');
    await session.post('Profiler.start');

    const {
      profile
    } = await session.post('Profiler.stop');

    const {
      callFrame: {
        url,
      },
    } = profile.nodes.find(({
      callFrame,
    }) => {
      return callFrame.url.includes(currentFilename);
    });
    session.disconnect();
    assert.deepStrictEqual(basename(url), currentFilename);
  }
  {
    // Ensure that even if a post function is slower than another, Promise.all will get it in order
    const session = new inspector.Session();
    session.connect();

    const sum1 = session.post('Runtime.evaluate', { expression: '2 + 2' });
    const exp = 'new Promise((r) => setTimeout(() => r(6), 100))';
    const sum2 = session.post('Runtime.evaluate', { expression: exp, awaitPromise: true });
    const sum3 = session.post('Runtime.evaluate', { expression: '4 + 4' });

    const results = (await Promise.all([
      sum1,
      sum2,
      sum3,
    ])).map(({ result: { value } }) => value);

    session.disconnect();
    assert.deepStrictEqual(results, [ 4, 6, 8 ]);
  }
})().then(common.mustCall());
