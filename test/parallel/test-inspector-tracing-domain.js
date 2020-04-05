'use strict';

const common = require('../common');

common.skipIfInspectorDisabled();
common.skipIfWorker(); // https://github.com/nodejs/node/issues/22767

const assert = require('assert');
const { Session } = require('inspector');

const session = new Session();

function compareIgnoringOrder(array1, array2) {
  const set = new Set(array1);
  const test = set.size === array2.length && array2.every((el) => set.has(el));
  assert.ok(test, `[${array1}] differs from [${array2}]`);
}

function post(message, data) {
  return new Promise((resolve, reject) => {
    session.post(message, data, (err, result) => {
      if (err)
        reject(new Error(JSON.stringify(err)));
      else
        resolve(result);
    });
  });
}

function generateTrace() {
  return new Promise((resolve) => setTimeout(() => {
    for (let i = 0; i < 1000000; i++) {
      'test' + i;
    }
    resolve();
  }, 1));
}

async function test() {
  // This interval ensures Node does not terminate till the test is finished.
  // Inspector session does not keep the node process running (e.g. it does not
  // have async handles on the main event loop). It is debatable whether this
  // should be considered a bug, and there are no plans to fix it atm.
  const interval = setInterval(() => {}, 5000);
  session.connect();
  let traceNotification = null;
  let tracingComplete = false;
  session.on('NodeTracing.dataCollected', (n) => traceNotification = n);
  session.on('NodeTracing.tracingComplete', () => tracingComplete = true);
  const { categories } = await post('NodeTracing.getCategories');
  compareIgnoringOrder(['node', 'node.async', 'node.bootstrap', 'node.fs.sync',
                        'node.perf', 'node.perf.usertiming',
                        'node.perf.timerify', 'v8'],
                       categories);

  const traceConfig = { includedCategories: ['v8'] };
  await post('NodeTracing.start', { traceConfig });

  for (let i = 0; i < 5; i++)
    await generateTrace();
  JSON.stringify(await post('NodeTracing.stop', { traceConfig }));
  session.disconnect();
  assert(traceNotification.params.value.length > 0);
  assert(tracingComplete);
  clearInterval(interval);
  console.log('Success');
}

test().then(common.mustCall());
