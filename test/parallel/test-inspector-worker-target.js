// Flags: --experimental-worker-inspection
'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const { Worker } = require('worker_threads');

common.skipIfInspectorDisabled();

const assert = require('assert');
const { Session } = require('inspector');

const session = new Session();

async function test() {
  session.connect();
  
  new Worker(fixtures.path("worker-script.mjs"), { type: 'module' });
  session.on("Target.targetCreated", common.mustCall(({ params }) => {
    const targetInfo = params.targetInfo;
    assert.strictEqual(targetInfo.type, "worker");
    assert.ok(targetInfo.url.includes("worker-script.mjs"));
    assert.strictEqual(targetInfo.targetId, "1");
  }));

  session.on("Target.attachedToTarget", common.mustCall(({ params }) => {
    assert.strictEqual(params.sessionId, "1");
  }));
}

test().then(common.mustCall());

