/* eslint-disable node-core/required-modules */
'use strict';
const assert = require('assert');
const fs = require('fs');
const os = require('os');
const path = require('path');

function findReports(pid, dir) {
  // Default filenames are of the form
  // report.<date>.<time>.<pid>.<tid>.<seq>.json
  const format = '^report\\.\\d+\\.\\d+\\.' + pid + '\\.\\d+\\.\\d+\\.json$';
  const filePattern = new RegExp(format);
  const files = fs.readdirSync(dir);
  const results = [];

  files.forEach((file) => {
    if (filePattern.test(file))
      results.push(path.join(dir, file));
  });

  return results;
}

function validate(report) {
  const data = fs.readFileSync(report, 'utf8');

  validateContent(data);
}

function validateContent(data) {
  try {
    _validateContent(data);
  } catch (err) {
    err.stack += `\n------\nFailing Report:\n${data}`;
    throw err;
  }
}

function _validateContent(data) {
  const isWindows = process.platform === 'win32';
  const report = JSON.parse(data);

  // Verify that all sections are present as own properties of the report.
  const sections = ['header', 'javascriptStack', 'nativeStack',
                    'javascriptHeap', 'libuv', 'environmentVariables',
                    'sharedObjects', 'resourceUsage'];
  if (!isWindows)
    sections.push('userLimits');

  if (report.uvthreadResourceUsage)
    sections.push('uvthreadResourceUsage');

  checkForUnknownFields(report, sections);
  sections.forEach((section) => {
    assert(report.hasOwnProperty(section));
    assert(typeof report[section] === 'object' && report[section] !== null);
  });

  // Verify the format of the header section.
  const header = report.header;
  const headerFields = ['event', 'trigger', 'filename', 'dumpEventTime',
                        'dumpEventTimeStamp', 'processId', 'commandLine',
                        'nodejsVersion', 'wordSize', 'arch', 'platform',
                        'componentVersions', 'release', 'osName', 'osRelease',
                        'osVersion', 'osMachine', 'host', 'glibcVersionRuntime',
                        'glibcVersionCompiler'];
  checkForUnknownFields(header, headerFields);
  assert.strictEqual(typeof header.event, 'string');
  assert.strictEqual(typeof header.trigger, 'string');
  assert(typeof header.filename === 'string' || header.filename === null);
  assert.notStrictEqual(new Date(header.dumpEventTime).toString(),
                        'Invalid Date');
  if (isWindows)
    assert.strictEqual(header.dumpEventTimeStamp, undefined);
  else
    assert(String(+header.dumpEventTimeStamp), header.dumpEventTimeStamp);

  assert(Number.isSafeInteger(header.processId));
  assert(Array.isArray(header.commandLine));
  header.commandLine.forEach((arg) => {
    assert.strictEqual(typeof arg, 'string');
  });
  assert.strictEqual(header.nodejsVersion, process.version);
  assert(Number.isSafeInteger(header.wordSize));
  assert.strictEqual(header.arch, os.arch());
  assert.strictEqual(header.platform, os.platform());
  assert.deepStrictEqual(header.componentVersions, process.versions);
  assert.deepStrictEqual(header.release, process.release);
  assert.strictEqual(header.osName, os.type());
  assert.strictEqual(header.osRelease, os.release());
  assert.strictEqual(typeof header.osVersion, 'string');
  assert.strictEqual(typeof header.osMachine, 'string');
  assert.strictEqual(header.host, os.hostname());

  // Verify the format of the javascriptStack section.
  checkForUnknownFields(report.javascriptStack, ['message', 'stack']);
  assert.strictEqual(typeof report.javascriptStack.message, 'string');
  if (report.javascriptStack.stack !== undefined) {
    assert(Array.isArray(report.javascriptStack.stack));
    report.javascriptStack.stack.forEach((frame) => {
      assert.strictEqual(typeof frame, 'string');
    });
  }

  // Verify the format of the nativeStack section.
  assert(Array.isArray(report.nativeStack));
  report.nativeStack.forEach((frame) => {
    assert(typeof frame === 'object' && frame !== null);
    checkForUnknownFields(frame, ['pc', 'symbol']);
    assert.strictEqual(typeof frame.pc, 'string');
    assert(/^0x[0-9a-f]+$/.test(frame.pc));
    assert.strictEqual(typeof frame.symbol, 'string');
  });

  // Verify the format of the javascriptHeap section.
  const heap = report.javascriptHeap;
  const jsHeapFields = ['totalMemory', 'totalCommittedMemory', 'usedMemory',
                        'availableMemory', 'memoryLimit', 'heapSpaces'];
  checkForUnknownFields(heap, jsHeapFields);
  assert(Number.isSafeInteger(heap.totalMemory));
  assert(Number.isSafeInteger(heap.totalCommittedMemory));
  assert(Number.isSafeInteger(heap.usedMemory));
  assert(Number.isSafeInteger(heap.availableMemory));
  assert(Number.isSafeInteger(heap.memoryLimit));
  assert(typeof heap.heapSpaces === 'object' && heap.heapSpaces !== null);
  const heapSpaceFields = ['memorySize', 'committedMemory', 'capacity', 'used',
                           'available'];
  Object.keys(heap.heapSpaces).forEach((spaceName) => {
    const space = heap.heapSpaces[spaceName];
    checkForUnknownFields(space, heapSpaceFields);
    heapSpaceFields.forEach((field) => {
      assert(Number.isSafeInteger(space[field]));
    });
  });

  // Verify the format of the resourceUsage section.
  const usage = report.resourceUsage;
  const resourceUsageFields = ['userCpuSeconds', 'kernelCpuSeconds',
                               'cpuConsumptionPercent', 'maxRss',
                               'pageFaults', 'fsActivity'];
  checkForUnknownFields(usage, resourceUsageFields);
  assert.strictEqual(typeof usage.userCpuSeconds, 'number');
  assert.strictEqual(typeof usage.kernelCpuSeconds, 'number');
  assert.strictEqual(typeof usage.cpuConsumptionPercent, 'number');
  assert(Number.isSafeInteger(usage.maxRss));
  assert(typeof usage.pageFaults === 'object' && usage.pageFaults !== null);
  checkForUnknownFields(usage.pageFaults, ['IORequired', 'IONotRequired']);
  assert(Number.isSafeInteger(usage.pageFaults.IORequired));
  assert(Number.isSafeInteger(usage.pageFaults.IONotRequired));
  assert(typeof usage.fsActivity === 'object' && usage.fsActivity !== null);
  checkForUnknownFields(usage.fsActivity, ['reads', 'writes']);
  assert(Number.isSafeInteger(usage.fsActivity.reads));
  assert(Number.isSafeInteger(usage.fsActivity.writes));

  // Verify the format of the uvthreadResourceUsage section, if present.
  if (report.uvthreadResourceUsage) {
    const usage = report.uvthreadResourceUsage;
    const threadUsageFields = ['userCpuSeconds', 'kernelCpuSeconds',
                               'cpuConsumptionPercent', 'fsActivity'];
    checkForUnknownFields(usage, threadUsageFields);
    assert.strictEqual(typeof usage.userCpuSeconds, 'number');
    assert.strictEqual(typeof usage.kernelCpuSeconds, 'number');
    assert.strictEqual(typeof usage.cpuConsumptionPercent, 'number');
    assert(typeof usage.fsActivity === 'object' && usage.fsActivity !== null);
    checkForUnknownFields(usage.fsActivity, ['reads', 'writes']);
    assert(Number.isSafeInteger(usage.fsActivity.reads));
    assert(Number.isSafeInteger(usage.fsActivity.writes));
  }

  // Verify the format of the libuv section.
  assert(Array.isArray(report.libuv));
  report.libuv.forEach((resource) => {
    assert.strictEqual(typeof resource.type, 'string');
    assert.strictEqual(typeof resource.address, 'string');
    assert(/^0x[0-9a-f]+$/.test(resource.address));
    assert.strictEqual(typeof resource.is_active, 'boolean');
    assert.strictEqual(typeof resource.is_referenced,
                       resource.type === 'loop' ? 'undefined' : 'boolean');
  });

  // Verify the format of the environmentVariables section.
  for (const [key, value] of Object.entries(report.environmentVariables)) {
    assert.strictEqual(typeof key, 'string');
    assert.strictEqual(typeof value, 'string');
  }

  // Verify the format of the userLimits section on non-Windows platforms.
  if (!isWindows) {
    const userLimitsFields = ['core_file_size_blocks', 'data_seg_size_kbytes',
                              'file_size_blocks', 'max_locked_memory_bytes',
                              'max_memory_size_kbytes', 'open_files',
                              'stack_size_bytes', 'cpu_time_seconds',
                              'max_user_processes', 'virtual_memory_kbytes'];
    checkForUnknownFields(report.userLimits, userLimitsFields);
    for (const [type, limits] of Object.entries(report.userLimits)) {
      assert.strictEqual(typeof type, 'string');
      assert(typeof limits === 'object' && limits !== null);
      checkForUnknownFields(limits, ['soft', 'hard']);
      assert(typeof limits.soft === 'number' || limits.soft === 'unlimited',
             `Invalid ${type} soft limit of ${limits.soft}`);
      assert(typeof limits.hard === 'number' || limits.hard === 'unlimited',
             `Invalid ${type} hard limit of ${limits.hard}`);
    }
  }

  // Verify the format of the sharedObjects section.
  assert(Array.isArray(report.sharedObjects));
  report.sharedObjects.forEach((sharedObject) => {
    assert.strictEqual(typeof sharedObject, 'string');
  });
}

function checkForUnknownFields(actual, expected) {
  Object.keys(actual).forEach((field) => {
    assert(expected.includes(field), `'${field}' not expected in ${expected}`);
  });
}

module.exports = { findReports, validate, validateContent };
