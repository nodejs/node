import * as common from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import * as snapshot from '../common/assertSnapshot.js';
import { describe, it } from 'node:test';
import { basename } from 'node:path';
import { pathToFileURL } from 'node:url';

describe('util output', { concurrency: !process.env.TEST_PARALLEL }, () => {
  function normalize(str) {
    const baseName = basename(process.argv0 || 'node', '.exe');
    return str.replaceAll(snapshot.replaceWindowsPaths(process.cwd()), '')
      .replaceAll(pathToFileURL(process.cwd()).pathname, '')
      .replaceAll('//', '*')
      .replaceAll(/\/(\w)/g, '*$1')
      .replaceAll('*test*', '*')
      .replaceAll('*fixtures*util*', '*')
      .replaceAll('file:**', 'file:*/')
      .replaceAll(`${baseName} --`, '* --');
  }

  const commonTransform = snapshot
    .transform(snapshot.replaceWindowsLineEndings, snapshot.replaceWindowsPaths);
  const defaultTransform = snapshot.transform(commonTransform, normalize, snapshot.replaceNodeVersion);

  const tests = [
    { name: 'util/util_inspect_error.js', transform: defaultTransform },
    { name: 'util/util-inspect-error-cause.js', transform: defaultTransform },
  ];

  for (const { name, transform } of tests) {
    it(name, async () => {
      await snapshot.spawnAndAssert(fixtures.path(name), transform);
    });
  }
});
