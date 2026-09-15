'use strict';
const common = require('../../common');
const assert = require('assert');
const dc = require('diagnostics_channel');
const test_async = require(`./build/${common.buildType}/test_async`);
const iterations = 500;

let x = 0;
const events = [];
const onThreadPoolWork = (event) => events.push(event);
const workDone = common.mustCall((status) => {
  assert.strictEqual(status, 0);
  if (x === 0) assert.strictEqual(events.length, 0);
  if (++x < iterations) {
    setImmediate(() => test_async.DoRepeatedWork(workDone));
  } else {
    dc.unsubscribe('threadpool.work.node_api', onThreadPoolWork);
    assert.strictEqual(events.length, iterations - 1);
  }
}, iterations);
// Subscribe after submission to verify subscription latching.
test_async.DoRepeatedWork(workDone);
dc.subscribe('threadpool.work.node_api', onThreadPoolWork);
