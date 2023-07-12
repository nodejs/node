import '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import * as snapshot from '../common/assertSnapshot.js';
import { describe, it } from 'node:test';

function replaceStackTrace(str) {
  return snapshot.replaceStackTrace(str, '$1at *$7\n');
}

function replaceStackTraceCustom(str) {
  return snapshot.replaceStackTrace(str, '$1at $4:$5:$6*\n');
}

describe('map output', { concurrency: true }, () => {
  function normalize(str) {
    return str
      .replaceAll(snapshot.replaceWindowsPaths(process.cwd()), '')
      .replaceAll('/test/fixtures/source-map/', '*')
      .replaceAll('\n\n', '\n')
      .replaceAll('Node.js', '\nNode.js')
      .replaceAll(process.version, '*');
  }

  const common = snapshot.transform(snapshot.replaceWindowsLineEndings, snapshot.replaceWindowsPaths);
  
  const defaultTransform = snapshot.transform(common, normalize);

  const noThrowCatchTransform = snapshot.transform(common, replaceStackTrace, normalize);
  const noErrorTabsTransform = snapshot.transform(common, replaceStackTraceCustom, normalize);
  
  const noUrlStringTransform = snapshot.transform(common, replaceStackTraceCustom, (str) => {
    return normalize(str)
      .replace(/((node:internal\/modules\/cjs\/loader:)(\d+):(\d+)\*)/, 'Module._compile ($2*)');
  });

  const noIcuTransform = snapshot.transform(common, replaceStackTraceCustom, (str) => {
    const replacement = (match) => match.replace(/[^(\n|\r)]/g, '*');

    return normalize(str)
      .replace(/(?<=\(")[^"]+/, replacement);
  });

  const tests = [
    { name: 'map/source_map_reference_error_tabs.js', transform: noErrorTabsTransform },
    { name: 'map/source_map_sourcemapping_url_string.js', transform: noUrlStringTransform },
    { name: 'map/source_map_throw_catch.js', transform: noThrowCatchTransform },
    { name: 'map/source_map_throw_icu.js', transform: noIcuTransform }
  ];
  for (const { name, transform } of tests) {
    it(name, async () => {
      await snapshot.spawnAndAssert(fixtures.path(name), transform ?? defaultTransform);
    });
  }
});
