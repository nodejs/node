'use strict';
const common = require('../common');

common.skipIfInspectorDisabled();

const assert = require('assert');
const { Session } = require('inspector');
const { promisify } = require('util');

const sleep = promisify(setTimeout);

async function test() {
  const inspector = new Session();
  inspector.connect();

  inspector.post('Profiler.enable');
  inspector.post('Profiler.start');

  await sleep(1000);

  const { profile } = await new Promise((resolve, reject) => {
    inspector.post('Profiler.stop', (err, params) => {
      if (err) return reject(err);
      resolve(params);
    });
  });

  let hasIdle = false;
  for (const node of profile.nodes) {
    if (node.callFrame.functionName === '(idle)') {
      hasIdle = true;
      break;
    }
  }
  assert(hasIdle);

  inspector.post('Profiler.disable');
  inspector.disconnect();
}

test().then(common.mustCall(() => {
  console.log('Done!');
}));
