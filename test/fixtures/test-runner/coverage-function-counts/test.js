'use strict';
const { writeFileSync } = require('node:fs');
const { join } = require('node:path');
const { pathToFileURL } = require('node:url');

function fn(functionName, startOffset, count, isBlockCoverage = false) {
  const endOffset = functionName ? startOffset + 15 : 64;
  return { functionName, ranges: [{ startOffset, endOffset, count }], isBlockCoverage };
}

function writeCoverage(id, functions) {
  writeFileSync(
    join(process.env.NODE_V8_COVERAGE, `coverage-${id}-0000000000000-0.json`),
    JSON.stringify({
      result: [{
        scriptId: '1',
        url: pathToFileURL(join(__dirname, 'lib.js')).href,
        functions: [fn('', 0, 1), ...functions],
      }],
    }),
  );
}

writeCoverage(1, [fn('a', 0, 1), fn('b', 16, 0), fn('c', 32, 1), fn('d', 48, 0, true)]);
writeCoverage(2, [fn('a', 0, 0), fn('b', 16, 1), fn('c', 32, 0, true), fn('d', 48, 1)]);
