// Flags: --enable-source-maps
'use strict';

const common = require('../common');
const assert = require('assert');
const { findSourceMap, SourceMap } = require('module');
const { readFileSync } = require('fs');

// It should throw with invalid args.
{
  [1, true, 'foo'].forEach((invalidArg) =>
    assert.throws(
      () => new SourceMap(invalidArg),
      {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError',
        message: 'The "payload" argument must be of type object.' +
               common.invalidArgTypeHelper(invalidArg)
      }
    )
  );
}

// `findSourceMap()` should return undefined when no source map is found.
{
  const files = [
    __filename,
    '',
    'invalid-file',
  ];
  for (const file of files) {
    const sourceMap = findSourceMap(file);
    assert.strictEqual(sourceMap, undefined);
  }
}

// findSourceMap() can lookup source-maps based on URIs, in the
// non-exceptional case.
{
  require('../fixtures/source-map/disk-relative-path.js');
  const sourceMap = findSourceMap(
    require.resolve('../fixtures/source-map/disk-relative-path.js')
  );
  const {
    originalLine,
    originalColumn,
    originalSource
  } = sourceMap.findEntry(0, 29);
  assert.strictEqual(originalLine, 2);
  assert.strictEqual(originalColumn, 4);
  assert(originalSource.endsWith('disk.js'));
  const {
    fileName,
    lineNumber,
    columnNumber,
  } = sourceMap.findOrigin(1, 30);
  assert.strictEqual(fileName, originalSource);
  assert.strictEqual(lineNumber, 3);
  assert.strictEqual(columnNumber, 6);
  assert(Array.isArray(sourceMap.lineLengths));
  assert(!sourceMap.lineLengths.some((len) => (typeof len !== 'number')));
}

// findSourceMap() can be used in Error.prepareStackTrace() to lookup
// source-map attached to error.
{
  let callSite;
  let sourceMap;
  Error.prepareStackTrace = (error, trace) => {
    const throwingRequireCallSite = trace[0];
    if (throwingRequireCallSite.getFileName().endsWith('typescript-throw.js')) {
      sourceMap = findSourceMap(throwingRequireCallSite.getFileName());
      callSite = throwingRequireCallSite;
    }
  };
  try {
    // Require a file that throws an exception, and has a source map.
    require('../fixtures/source-map/typescript-throw.js');
  } catch (err) {
    // eslint-disable-next-line no-unused-expressions
    err.stack; // Force prepareStackTrace() to be called.
  }
  assert(callSite);
  assert(sourceMap);
  const {
    generatedLine,
    generatedColumn,
    originalLine,
    originalColumn,
    originalSource
  } = sourceMap.findEntry(
    callSite.getLineNumber() - 1,
    callSite.getColumnNumber() - 1
  );

  assert.strictEqual(generatedLine, 19);
  assert.strictEqual(generatedColumn, 14);

  assert.strictEqual(originalLine, 17);
  assert.strictEqual(originalColumn, 10);
  assert(originalSource.endsWith('typescript-throw.ts'));

  const {
    fileName,
    lineNumber,
    columnNumber,
  } = sourceMap.findOrigin(
    callSite.getLineNumber(),
    callSite.getColumnNumber()
  );
  assert.strictEqual(fileName, originalSource);
  assert.strictEqual(lineNumber, 18);
  assert.strictEqual(columnNumber, 11);
}

// SourceMap can be instantiated with Source Map V3 object as payload.
{
  const payload = JSON.parse(readFileSync(
    require.resolve('../fixtures/source-map/disk.map'), 'utf8'
  ));
  const lineLengths = readFileSync(
    require.resolve('../fixtures/source-map/disk.map'), 'utf8'
  ).replace(/\n$/, '').split('\n').map((l) => l.length);
  const sourceMap = new SourceMap(payload, { lineLengths });
  const {
    originalLine,
    originalColumn,
    originalSource
  } = sourceMap.findEntry(0, 29);
  assert.strictEqual(originalLine, 2);
  assert.strictEqual(originalColumn, 4);
  assert(originalSource.endsWith('disk.js'));
  const sourceMapLineLengths = sourceMap.lineLengths;
  for (let i = 0; i < sourceMapLineLengths.length; i++) {
    assert.strictEqual(sourceMapLineLengths[i], lineLengths[i]);
  }
  assert.strictEqual(sourceMapLineLengths.length, lineLengths.length);
  // The stored payload should be a clone:
  assert.strictEqual(payload.mappings, sourceMap.payload.mappings);
  assert.notStrictEqual(payload, sourceMap.payload);
  assert.strictEqual(payload.sources[0], sourceMap.payload.sources[0]);
  assert.notStrictEqual(payload.sources, sourceMap.payload.sources);
}

// findEntry() and findOrigin() must return empty object instead of
// error when receiving a malformed mappings.
{
  const payload = JSON.parse(readFileSync(
    require.resolve('../fixtures/source-map/disk.map'), 'utf8'
  ));
  payload.mappings = ';;;;;;;;;';

  const sourceMap = new SourceMap(payload);
  const result = sourceMap.findEntry(0, 5);
  assert.strictEqual(typeof result, 'object');
  assert.strictEqual(Object.keys(result).length, 0);
  const origin = sourceMap.findOrigin(0, 5);
  assert.strictEqual(typeof origin, 'object');
  assert.strictEqual(Object.keys(origin).length, 0);
}

// SourceMap can be instantiated with Index Source Map V3 object as payload.
{
  const payload = JSON.parse(readFileSync(
    require.resolve('../fixtures/source-map/disk-index.map'), 'utf8'
  ));
  const sourceMap = new SourceMap(payload);
  const {
    originalLine,
    originalColumn,
    originalSource
  } = sourceMap.findEntry(0, 29);
  assert.strictEqual(originalLine, 2);
  assert.strictEqual(originalColumn, 4);
  assert(originalSource.endsWith('section.js'));
  // The stored payload should be a clone:
  assert.strictEqual(payload.mappings, sourceMap.payload.mappings);
  assert.notStrictEqual(payload, sourceMap.payload);
  assert.strictEqual(payload.sources[0], sourceMap.payload.sources[0]);
  assert.notStrictEqual(payload.sources, sourceMap.payload.sources);
}

// Test various known decodings to ensure decodeVLQ works correctly.
{
  function makeMinimalMap(column) {
    return {
      sources: ['test.js'],
      // Mapping from the 0th line, 0th column of the output file to the 0th
      // source file, 0th line, ${column}th column.
      mappings: `AAA${column}`,
    };
  }
  const knownDecodings = {
    'A': 0,
    'B': -2147483648,
    'C': 1,
    'D': -1,
    'E': 2,
    'F': -2,

    // 2^31 - 1, maximum values
    '+/////D': 2147483647,
    '8/////D': 2147483646,
    '6/////D': 2147483645,
    '4/////D': 2147483644,
    '2/////D': 2147483643,
    '0/////D': 2147483642,

    // -2^31 + 1, minimum values
    '//////D': -2147483647,
    '9/////D': -2147483646,
    '7/////D': -2147483645,
    '5/////D': -2147483644,
    '3/////D': -2147483643,
    '1/////D': -2147483642,
  };

  for (const column in knownDecodings) {
    const sourceMap = new SourceMap(makeMinimalMap(column));
    const { originalColumn } = sourceMap.findEntry(0, 0);
    assert.strictEqual(originalColumn, knownDecodings[column]);
  }
}

// Test that generated columns are sorted when a negative offset is
// observed, see: https://github.com/mozilla/source-map/pull/92
{
  function makeMinimalMap(generatedColumns, originalColumns) {
    return {
      sources: ['test.js'],
      // Mapping from the 0th line, ${g}th column of the output file to the 0th
      // source file, 0th line, ${column}th column.
      mappings: generatedColumns.map((g, i) => `${g}AA${originalColumns[i]}`)
        .join(',')
    };
  }
  // U = 10
  // F = -2
  // A = 0
  // E = 2
  const sourceMap = new SourceMap(makeMinimalMap(
    ['U', 'F', 'F'],
    ['A', 'E', 'E']
  ));
  assert.strictEqual(sourceMap.findEntry(0, 6).originalColumn, 4);
  assert.strictEqual(sourceMap.findEntry(0, 8).originalColumn, 2);
  assert.strictEqual(sourceMap.findEntry(0, 10).originalColumn, 0);
}
