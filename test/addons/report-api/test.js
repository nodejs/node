'use strict';

const common = require('../../common');
const assert = require('assert');
const path = require('path');
const helper = require('../../common/report.js');
const tmpdir = require('../../common/tmpdir');

const binding = path.resolve(__dirname, `./build/${common.buildType}/binding`);
const addon = require(binding);

function myAddonMain(method, hasJavaScriptFrames) {
  tmpdir.refresh();
  process.report.directory = tmpdir.path;

  addon[method]();

  const reports = helper.findReports(process.pid, tmpdir.path);
  assert.strictEqual(reports.length, 1);

  const report = reports[0];
  helper.validate(report);

  const content = require(report);
  assert.strictEqual(content.header.event, 'FooMessage');
  assert.strictEqual(content.header.trigger, 'BarTrigger');

  // Check that the javascript stack is present.
  if (hasJavaScriptFrames) {
    assert.strictEqual(content.javascriptStack.stack.findIndex((frame) => frame.match('myAddonMain')), 0);
  } else {
    assert.strictEqual(content.javascriptStack, undefined);
  }
}

const methods = [
  ['triggerReport', true],
  ['triggerReportNoIsolate', false],
  ['triggerReportEnv', true],
  ['triggerReportNoEnv', false],
];
for (const [method, hasJavaScriptFrames] of methods) {
  myAddonMain(method, hasJavaScriptFrames);
}
