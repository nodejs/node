// Flags: --expose-gc --expose-internals
'use strict';

require('../common');

const assert = require('assert');
const {
  monitorEventLoopDelay,
  monitorEventLoopIdleness
} = require('perf_hooks');

for (const method of [ monitorEventLoopDelay, monitorEventLoopIdleness ]) {
  {
    const histogram = method();
    assert(histogram);
    assert(histogram.enable());
    assert(!histogram.enable());
    histogram.reset();
    assert(histogram.disable());
    assert(!histogram.disable());
  }

  {
    [null, 'a', 1, false, Infinity].forEach((i) => {
      assert.throws(
        () => method(i),
        {
          name: 'TypeError',
          code: 'ERR_INVALID_ARG_TYPE'
        }
      );
    });

    [null, 'a', false, {}, []].forEach((i) => {
      assert.throws(
        () => method({ resolution: i }),
        {
          name: 'TypeError',
          code: 'ERR_INVALID_ARG_TYPE'
        }
      );
    });

    [-1, 0, Infinity].forEach((i) => {
      assert.throws(
        () => method({ resolution: i }),
        {
          name: 'RangeError',
          code: 'ERR_INVALID_OPT_VALUE'
        }
      );
    });

    ['a', false, {}, []].forEach((i) => {
      assert.throws(
        () => method().percentile(i),
        {
          name: 'TypeError',
          code: 'ERR_INVALID_ARG_TYPE'
        }
      );
    });
    [-1, 0, 101].forEach((i) => {
      assert.throws(
        () => method().percentile(i),
        {
          name: 'RangeError',
          code: 'ERR_INVALID_ARG_VALUE'
        }
      );
    });
  }
}
