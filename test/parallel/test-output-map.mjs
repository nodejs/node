import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import * as snapshot from '../common/assertSnapshot.js';
import { describe, it } from 'node:test';

function replaceNodeVersion(str) {
  return str.replaceAll(process.version, '*');
}

function replaceStackTrace(str) {
  return snapshot.replaceStackTrace(str, '$1at *$7\n');
}

describe('map output', { concurrency: true }, () => {
  function normalize(str) {
    return str.replaceAll(snapshot.replaceWindowsPaths(process.cwd()), '').replaceAll('//', '*').replaceAll(/\/(\w)/g, '*$1').replaceAll('*test*', '*').replaceAll('*fixtures*source-map*', '*').replaceAll('file:**', 'file:*/');
  }

  function normalizeNoNumbers(str) {
    return normalize(str).replaceAll(/\d+:\d+/g, '*:*').replaceAll(/:\d+/g, ':*').replaceAll('*fixtures*source-map*', '*');
  }
  function normalizeSpecialCharacters(str) {
    const replacement = (match) => match.replace(/./g, '*').replace(/\s\*/g, '');
    return normalizeNoNumbers(str).replace(/(?<=\(")[^"]+/, replacement);
  }
  const common = snapshot
    .transform(snapshot.replaceWindowsLineEndings, snapshot.replaceWindowsPaths, replaceNodeVersion);
  const defaultTransform = snapshot.transform(common, normalize);
  const noNumTransform = snapshot.transform(common, normalizeNoNumbers);
  const customTransform = snapshot.transform(common, replaceStackTrace, normalizeNoNumbers);
  const specialTransform = snapshot.transform(common, replaceStackTrace, normalizeSpecialCharacters);

  const tests = [
    { name: 'map/source_map_enclosing_function.js' },
    { name: 'map/source_map_reference_error_tabs.js' },
    { name: 'map/source_map_sourcemapping_url_string.js', transform: noNumTransform },
    { name: 'map/source_map_throw_catch.js', transform: customTransform },
    { name: 'map/source_map_throw_icu.js', transform: specialTransform },
    { name: 'map/source_map_throw_set_immediate.js', transform: customTransform },
  ];
  for (const { name, transform } of tests) {
    it(name, async () => {
      await snapshot.spawnAndAssert(fixtures.path(name), transform ?? defaultTransform, { tty: true });
    });
  }
});
