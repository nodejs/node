'use strict';

const common = require('../../common');
const assert = require('assert');
const path = require('path');
const helper = require('../../common/report.js');
const tmpdir = require('../../common/tmpdir');

const binding = path.resolve(__dirname, `./build/${common.buildType}/binding`);
const addon = require(binding);

function myAddonMain(method, { hasIsolate, hasEnv }) {
  tmpdir.refresh();
  process.report.directory = tmpdir.path;

  addon[method]();

  const reports = helper.findReports(process.pid, tmpdir.path);
  assert.strictEqual(reports.length, 1);

  const report = reports[0];
  helper.validate(report, [
    ['header.event', 'FooMessage'],
    ['header.trigger', 'BarTrigger'],
  ]);

  const content = require(report);

  // Check that the javascript stack is present.
  if (hasIsolate) {
    assert.strictEqual(content.javascriptStack.stack.findIndex((frame) => frame.match('myAddonMain')), 0);
  } else {
    assert.strictEqual(content.javascriptStack, undefined);
  }

  if (hasEnv) {
    assert.strictEqual(content.header.threadId, 0);
  } else {
    assert.strictEqual(content.header.threadId, null);
  }
}

const methods = [
  ['triggerReport', true, true],
  ['triggerReportNoIsolate', false, false],
  ['triggerReportEnv', true, true],
  ['triggerReportNoEnv', false, false],
  ['triggerReportNoContext', true, false],
  ['triggerReportNewContext', true, false],
];
for (const [method, hasIsolate, hasEnv] of methods) {
  myAddonMain(method, { hasIsolate, hasEnv });
}
