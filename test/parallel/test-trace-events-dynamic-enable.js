'use strict';

const common = require('../common');

common.skipIfInspectorDisabled();
common.skipIfWorker(); // https://github.com/nodejs/node/issues/22767

const assert = require('assert');
const { performance } = require('perf_hooks');
const { Session } = require('inspector');

const session = new Session();

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

async function test() {
  session.connect();

  const events = [];
  let tracingComplete = false;
  session.on('NodeTracing.dataCollected', (n) => {
    assert.ok(n && n.params && n.params.value);
    events.push(...n.params.value);  // append the events.
  });
  session.on('NodeTracing.tracingComplete', () => tracingComplete = true);

  // Generate a node.perf event before tracing is enabled.
  performance.mark('mark1');

  const traceConfig = { includedCategories: ['node.perf'] };
  await post('NodeTracing.start', { traceConfig });

  // Generate a node.perf event after tracing is enabled. This should be the
  // mark event captured.
  performance.mark('mark2');

  await post('NodeTracing.stop', { traceConfig });

  performance.mark('mark3');

  session.disconnect();

  assert.ok(tracingComplete);

  const marks = events.filter((t) => null !== /node\.perf\.usertim/.exec(t.cat));
  assert.strictEqual(marks.length, 1);
  assert.strictEqual(marks[0].name, 'mark2');
}

test();
