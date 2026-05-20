'use strict';

const os = require('node:os');

// On AIX, V8's OS::DecommitPages() has an inherent race condition caused by
// AIX's non-POSIX MAP_FIXED behavior. The implementation must munmap() then
// mmap(), and another thread can steal the address range in between. The
// Blob.arrayBuffer() tests trigger this by creating enough GC pressure
// (especially the concurrent reads test) to hit the race window.
// See deps/v8/src/base/platform/platform-aix.cc, lines 168-203.
const isAIX = os.type() === 'AIX';

const conditionalSkips = {};

if (isAIX) {
  conditionalSkips['Blob-array-buffer.any.js'] = {
    skip: 'V8 DecommitPages race condition on AIX (munmap/mmap non-atomic)',
  };
}

module.exports = {
  ...conditionalSkips,
  'Blob-constructor-dom.window.js': {
    skip: 'Depends on DOM API',
  },
  'Blob-constructor.any.js': {
    fail: {
      flaky: [
        'Passing typed arrays as elements of the blobParts array should work.',
        'Passing a Float16Array as element of the blobParts array should work.',
        'Passing a Float64Array as element of the blobParts array should work.',
        'Passing BigInt typed arrays as elements of the blobParts array should work.',
      ],
    },
  },
  'Blob-in-worker.worker.js': {
    skip: 'Depends on Web Workers API',
  },
  'Blob-slice.any.js': {
    fail: {
      expected: [
        'Slicing test: slice (1,1).',
        'Slicing test: slice (1,3).',
        'Slicing test: slice (1,5).',
        'Slicing test: slice (1,7).',
        'Slicing test: slice (1,8).',
        'Slicing test: slice (1,9).',
      ],
    },
  },
};
