import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import * as snapshot from '../common/assertSnapshot.js';
import { describe, it } from 'node:test';

function replaceStackTrace(str) {
  return snapshot.replaceStackTrace(str, '$1at *$7\n');
}

describe('map output', { concurrency: true }, () => {
  function normalize(str) {
    return str
      .replaceAll(snapshot.replaceWindowsPaths(process.cwd()), '')
      .replaceAll('//', '*')
      .replaceAll(process.version, '*')
      .replaceAll(/\/(\w)/g, '*$1')
      .replaceAll('*test*', '*')
      .replaceAll('*fixtures*source-map*', '*')
      .replaceAll(/:\d+/g, ':*');
  }

  function normalizeSpecialCharacters(str) {
    const replacement = (match) => match.replace(/[^(\n|\r)]/g, '*').replace(/\s\*/g, '');
    return normalize(str).replace(/(?<=\(")[^"]+/, replacement);
  }

  const common = snapshot
    .transform(snapshot.replaceWindowsLineEndings, snapshot.replaceWindowsPaths);
  const defaultTransform = snapshot.transform(common, normalize);
  const noStackTraceTransform = snapshot.transform(common, replaceStackTrace, normalize);
  const noSpecialCharacterTransform = snapshot.transform(common, replaceStackTrace, normalizeSpecialCharacters);

  const tests = [
    { name: 'map/source_map_reference_error_tabs.js', transform: noStackTraceTransform },
    { name: 'map/source_map_sourcemapping_url_string.js', transform: noStackTraceTransform },
    { name: 'map/source_map_throw_catch.js', transform: noStackTraceTransform },
    { name: 'map/source_map_throw_icu.js', transform: noSpecialCharacterTransform }
  ];
  for (const { name, transform } of tests) {
    it(name, async () => {
      await snapshot.spawnAndAssert(fixtures.path(name), transform ?? defaultTransform);
    });
  }
});
