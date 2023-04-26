import * as common from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import * as snapshot from '../common/assertSnapshot.js';
import * as path from 'node:path';
import { describe, it } from 'node:test';

function replaceNodeVersion(str) {
  return str.replaceAll(process.version, '*');
}

describe('sourcemaps output', { concurrency: true }, () => {
  function normalize(str) {
    const result = str
    .replaceAll(snapshot.replaceWindowsPaths(process.cwd()), '')
    .replaceAll('//', '*')
    .replaceAll('/Users/bencoe/oss/coffee-script-test', '')
    .replaceAll(/\/(\w)/g, '*$1')
    .replaceAll('*test*', '*')
    .replaceAll('*fixtures*source-map*', '*');
    if (common.isWindows) {
      const currentDeviceLetter = path.parse(process.cwd()).root.substring(0, 2);
      const regex = new RegExp(`${currentDeviceLetter}:/?`, 'gi');
      return result.replaceAll(regex, '');
    }
    return result;
  }
  const defaultTransform = snapshot
    .transform(snapshot.replaceWindowsLineEndings, snapshot.replaceWindowsPaths, replaceNodeVersion, normalize);

  const tests = [
    { name: 'source-map/output/source_map_disabled_by_api.js' },
    { name: 'source-map/output/source_map_enabled_by_api.js' },
    { name: 'source-map/output/source_map_eval.js' },
    { name: 'source-map/output/source_map_no_source_file.js' },
    { name: 'source-map/output/source_map_throw_first_tick.js' },
  ];
  for (const { name, transform } of tests) {
    it(name, async () => {
      await snapshot.spawnAndAssert(fixtures.path(name), transform ?? defaultTransform);
    });
  }
});
