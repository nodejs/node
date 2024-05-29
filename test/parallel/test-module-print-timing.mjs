import '../common/index.mjs';
import assert from 'node:assert';
import { readFile } from 'node:fs/promises';
import { it } from 'node:test';
import tmpdir from '../common/tmpdir.js';
import { spawnSyncAndAssert } from '../common/child_process.js';

tmpdir.refresh();

it('should print the timing information for cjs', () => {
  const result = spawnSyncAndAssert(process.execPath, ['--eval', 'require("url");'], {
    cwd: tmpdir.path,
    env: {
      ...process.env,
      NODE_DEBUG: 'module_timer',
    },
  }, {
    stdout: '',
    stderr: (result) => result.includes('MODULE_TIMER'),
  });

  const firstLine = result.stderr.split('\n').find((line) => line.includes('[url]'));

  assert.notStrictEqual(firstLine, undefined);
  assert.ok(firstLine.includes('MODULE_TIMER'), `Not found MODULE_TIMER on ${firstLine}`);
  assert.ok(firstLine.includes('[url]:'), `Not found [url]: on ${firstLine}`);
  assert.ok(firstLine.endsWith('ms'), `Not found ms on ${firstLine}`);
});

it('should write tracing information for cjs', async () => {
  const outputFile = tmpdir.resolve('output-trace.log');

  spawnSyncAndAssert(process.execPath, [
    '--trace-event-categories',
    'node.module_timer',
    '--trace-event-file-pattern',
    outputFile,
    '--eval',
    'require("url");',
  ], {
    cwd: tmpdir.path,
  }, {
    stdout: '',
    stderr: '',
  });

  const expectedMimeTypes = ['b', 'e'];
  const outputFileContent = await readFile(outputFile, 'utf-8');
  const outputFileJson = JSON.parse(outputFileContent).traceEvents;
  const urlTraces = outputFileJson.filter((trace) => trace.name === "require('url')");

  assert.strictEqual(urlTraces.length, 2);

  for (const trace of urlTraces) {
    assert.strictEqual(trace.ph, expectedMimeTypes.shift());
  }
});

it('should write tracing & print logs for cjs', async () => {
  const outputFile = tmpdir.resolve('output-trace-and-log.log');

  const result = spawnSyncAndAssert(process.execPath, [
    '--trace-event-categories',
    'node.module_timer',
    '--trace-event-file-pattern',
    outputFile,
    '--eval',
    'require("url");',
  ], {
    cwd: tmpdir.path,
    env: {
      ...process.env,
      NODE_DEBUG: 'module_timer',
    },
  }, {
    stdout: '',
    stderr: (result) => result.includes('MODULE_TIMER'),
  });

  const firstLine = result.stderr.split('\n').find((line) => line.includes('[url]'));

  assert.notStrictEqual(firstLine, undefined);
  assert.ok(firstLine.includes('MODULE_TIMER'), `Not found MODULE_TIMER on ${firstLine}`);
  assert.ok(firstLine.includes('[url]:'), `Not found [url]: on ${firstLine}`);
  assert.ok(firstLine.endsWith('ms'), `Not found ms on ${firstLine}`);

  const expectedMimeTypes = ['b', 'e'];
  const outputFileContent = await readFile(outputFile, 'utf-8');
  const outputFileJson = JSON.parse(outputFileContent).traceEvents;
  const urlTraces = outputFileJson.filter((trace) => trace.name === "require('url')");

  for (const trace of urlTraces) {
    assert.strictEqual(trace.ph, expectedMimeTypes.shift());
  }
});
