'use strict';

require('../common');
const assert = require('assert');
const { GCProfiler } = require('v8');

{
  const profiler = new GCProfiler();
  profiler.start();

  const result = profiler[Symbol.dispose]();

  assert.strictEqual(result, undefined);
  assert.strictEqual(profiler.stop(), undefined);
}

{
  const profiler = new GCProfiler();
  profiler.start();

  profiler[Symbol.dispose]();
  // Repeat invocations should not throw
  profiler[Symbol.dispose]();

  assert.strictEqual(profiler.stop(), undefined);
}
