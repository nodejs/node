'use strict';

// Test producing a report via API call, using the no-hooks/no-signal interface.
require('../common');
const assert = require('assert');
const { spawnSync } = require('child_process');
const fs = require('fs');
const path = require('path');
const helper = require('../common/report');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();
process.report.directory = tmpdir.path;

function validate() {
  const reports = helper.findReports(process.pid, tmpdir.path);
  assert.strictEqual(reports.length, 1);
  helper.validate(reports[0]);
  fs.unlinkSync(reports[0]);
  return reports[0];
}

{
  // Test with no arguments.
  process.report.writeReport();
  validate();
}

{
  // Test with an error argument.
  process.report.writeReport(new Error('test error'));
  validate();
}

{
  // Test with an error with one line stack
  const error = new Error();
  error.stack = 'only one line';
  process.report.writeReport(error);
  validate();
}

{
  // Test with a file argument.
  const file = process.report.writeReport('custom-name-1.json');
  const absolutePath = path.join(tmpdir.path, file);
  assert.strictEqual(helper.findReports(process.pid, tmpdir.path).length, 0);
  assert.strictEqual(file, 'custom-name-1.json');
  helper.validate(absolutePath);
  fs.unlinkSync(absolutePath);
}

{
  // Test with file and error arguments.
  const file = process.report.writeReport('custom-name-2.json',
                                          new Error('test error'));
  const absolutePath = path.join(tmpdir.path, file);
  assert.strictEqual(helper.findReports(process.pid, tmpdir.path).length, 0);
  assert.strictEqual(file, 'custom-name-2.json');
  helper.validate(absolutePath);
  fs.unlinkSync(absolutePath);
}

{
  // Test with a filename option.
  process.report.filename = 'custom-name-3.json';
  const file = process.report.writeReport();
  assert.strictEqual(helper.findReports(process.pid, tmpdir.path).length, 0);
  const filename = path.join(process.report.directory, 'custom-name-3.json');
  assert.strictEqual(file, process.report.filename);
  helper.validate(filename);
  fs.unlinkSync(filename);
}

// Test with an invalid file argument.
[null, 1, Symbol(), function() {}].forEach((file) => {
  assert.throws(() => {
    process.report.writeReport(file);
  }, { code: 'ERR_INVALID_ARG_TYPE' });
});

// Test with an invalid error argument.
[null, 1, Symbol(), function() {}, 'foo'].forEach((error) => {
  assert.throws(() => {
    process.report.writeReport('file', error);
  }, { code: 'ERR_INVALID_ARG_TYPE' });
});

{
  // Test the special "stdout" filename.
  const args = ['-e', 'process.report.writeReport("stdout")'];
  const child = spawnSync(process.execPath, args, { cwd: tmpdir.path });
  assert.strictEqual(child.status, 0);
  assert.strictEqual(child.signal, null);
  assert.strictEqual(helper.findReports(child.pid, tmpdir.path).length, 0);
  helper.validateContent(child.stdout.toString());
}

{
  // Test the special "stderr" filename.
  const args = ['-e', 'process.report.writeReport("stderr")'];
  const child = spawnSync(process.execPath, args, { cwd: tmpdir.path });
  assert.strictEqual(child.status, 0);
  assert.strictEqual(child.signal, null);
  assert.strictEqual(child.stdout.toString().trim(), '');
  assert.strictEqual(helper.findReports(child.pid, tmpdir.path).length, 0);
  const report = child.stderr.toString().split('Node.js report completed')[0];
  helper.validateContent(report);
}

{
  // Test the case where the report file cannot be opened.
  const reportDir = path.join(tmpdir.path, 'does', 'not', 'exist');
  const args = [`--report-directory=${reportDir}`,
                '-e',
                'process.report.writeReport()'];
  const child = spawnSync(process.execPath, args, { cwd: tmpdir.path });

  assert.strictEqual(child.status, 0);
  assert.strictEqual(child.signal, null);
  assert.strictEqual(child.stdout.toString().trim(), '');
  const stderr = child.stderr.toString();
  assert(stderr.includes('Failed to open Node.js report file:'));
}
