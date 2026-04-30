'use strict';

const common = require('../common.js');
const fs = require('node:fs');
const path = require('node:path');

// Cannot use test/ffi/ffi-test-common.js because it requires test/common
// (the test harness module). Construct the path directly.
const libraryPath = path.join(__dirname, '..', '..', 'test', 'ffi',
                              'fixture_library', 'build', common.buildType,
                              process.platform === 'win32' ? 'ffi_test_library.dll' :
                                process.platform === 'darwin' ? 'ffi_test_library.dylib' :
                                  'ffi_test_library.so');

function ensureFixtureLibrary() {
  if (!fs.existsSync(libraryPath)) {
    throw new Error(
      `Missing FFI fixture library: ${libraryPath}. ` +
      'Build it with `tools/test.py test/ffi/test-ffi-calls.js` first.',
    );
  }
}

module.exports = { libraryPath, ensureFixtureLibrary };
