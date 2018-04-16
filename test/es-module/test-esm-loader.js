'use strict';
// Flags: --expose-internals

// This test ensures that resolve and search throw errors appropriately

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const { join } = require('path');
const resolve = require('internal/modules/esm/default_resolve');
const { writeFileSync, unlinkSync } = require('fs');

const { search } = resolve;
const unsupported = join(
  tmpdir.path,
  'module-with-extension-that-is.unsupported');
tmpdir.refresh();
writeFileSync(unsupported, '');
common.expectsError(
  () => resolve(unsupported, 'file://' + require.resolve(__filename)),
  {
    code: 'ERR_UNKNOWN_FILE_EXTENSION',
    type: TypeError,
    message: 'Unknown file extension: ' + unsupported
  }
);
unlinkSync(unsupported);

common.expectsError(
  () => search('target', undefined),
  {
    code: 'ERR_MISSING_MODULE',
    type: Error,
    message: 'Cannot find module target'
  }
);
