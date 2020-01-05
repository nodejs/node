// Flags: --enable-source-maps
'use strict';

require('../common');
const assert = require('assert');
const { findSourceMap, SourceMap } = require('module');
const { readFileSync } = require('fs');

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
}

// findSourceMap() can be used in Error.prepareStackTrace() to lookup
// source-map attached to error.
{
  let callSite;
  let sourceMap;
  Error.prepareStackTrace = (error, trace) => {
    const throwingRequireCallSite = trace[0];
    if (throwingRequireCallSite.getFileName().endsWith('typescript-throw.js')) {
      sourceMap = findSourceMap(throwingRequireCallSite.getFileName(), error);
      callSite = throwingRequireCallSite;
    }
  };
  try {
    // Require a file that throws an exception, and has a source map.
    require('../fixtures/source-map/typescript-throw.js');
  } catch (err) {
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
}

// SourceMap can be instantiated with Source Map V3 object as payload.
{
  const payload = JSON.parse(readFileSync(
    require.resolve('../fixtures/source-map/disk.map'), 'utf8'
  ));
  const sourceMap = new SourceMap(payload);
  const {
    originalLine,
    originalColumn,
    originalSource
  } = sourceMap.findEntry(0, 29);
  assert.strictEqual(originalLine, 2);
  assert.strictEqual(originalColumn, 4);
  assert(originalSource.endsWith('disk.js'));
  assert.strictEqual(payload, sourceMap.payload);
}
