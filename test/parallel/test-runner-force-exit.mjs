import * as common from '../common/index.mjs';
import * as tmpdir from '../common/tmpdir.js';
import * as fixtures from '../common/fixtures.mjs';
import * as snapshot from '../common/assertSnapshot.js';
import { describe, it } from 'node:test';
import { hostname } from 'node:os';
import { fileURLToPath } from 'node:url';
import { readFile } from 'node:fs/promises';

function replaceTestDuration(str) {
  return str
    .replaceAll(/duration_ms: [0-9.]+/g, 'duration_ms: *')
    .replaceAll(/duration_ms [0-9.]+/g, 'duration_ms *');
}

const root = fileURLToPath(new URL('../..', import.meta.url)).slice(0, -1);

const color = '(\\[\\d+m)';
const stackTraceBasePath = new RegExp(`${color}\\(${root.replaceAll(/[\\^$*+?.()|[\]{}]/g, '\\$&')}/?${color}(.*)${color}\\)`, 'g');

function replaceSpecDuration(str) {
  return str
    .replaceAll(/[0-9.]+ms/g, '*ms')
    .replaceAll(/duration_ms [0-9.]+/g, 'duration_ms *')
    .replace(stackTraceBasePath, '$3');
}

function replaceJunitDuration(str) {
  return str
    .replaceAll(/time="[0-9.]+"/g, 'time="*"')
    .replaceAll(/duration_ms [0-9.]+/g, 'duration_ms *')
    .replaceAll(hostname(), 'HOSTNAME')
    .replace(stackTraceBasePath, '$3');
}

function removeWindowsPathEscaping(str) {
  return common.isWindows ? str.replaceAll(/\\\\/g, '\\') : str;
}

function replaceTestLocationLine(str) {
  return str.replaceAll(/(js:)(\d+)(:\d+)/g, '$1(LINE)$3');
}

const defaultTransform = snapshot.transform(
  snapshot.replaceWindowsLineEndings,
  snapshot.replaceStackTrace,
  removeWindowsPathEscaping,
  snapshot.replaceFullPaths,
  snapshot.replaceWindowsPaths,
  replaceTestDuration,
  replaceTestLocationLine,
);

const transformers = {
  junit: snapshot.transform(
    replaceJunitDuration,
    snapshot.replaceWindowsLineEndings,
    snapshot.replaceStackTrace,
    snapshot.replaceWindowsPaths,
  ),
  spec: snapshot.transform(
    replaceSpecDuration,
    snapshot.replaceWindowsLineEndings,
    snapshot.replaceStackTrace,
    snapshot.replaceWindowsPaths,
  ),
  dot: snapshot.transform(
    replaceSpecDuration,
    snapshot.replaceWindowsLineEndings,
    snapshot.replaceStackTrace,
    snapshot.replaceWindowsPaths,
  ),
};

describe('test runner output', { concurrency: true }, async () => {
  tmpdir.refresh();
  for (const reporter of ['dot', 'junit', 'spec', 'tap']) {
    await it(reporter, async (t) => {
      const output = tmpdir.resolve(`${reporter}.out`);
      const spawned = await common.spawnPromisified(
        process.execPath,
        [
          '--test',
          '--test-force-exit',
          `--test-reporter=${reporter}`,
          `--test-reporter-destination=${output}`,
          fixtures.path('test-runner/output/test-runner-force-exit.js'),
        ],
      );
      t.assert.deepStrictEqual(spawned, { code: 1, signal: null, stderr: '', stdout: '' });
      const transformer = transformers[reporter] || defaultTransform;
      const outputData = await readFile(output, 'utf-8');
      await snapshot.assertSnapshot(transformer(outputData), fixtures.path(`test-runner/output/test-runner-force-exit-${reporter}.output`));
    });
  }
  tmpdir.refresh();
});
