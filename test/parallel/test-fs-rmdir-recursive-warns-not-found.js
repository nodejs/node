'use strict';
const common = require('../common');
const tmpdir = require('../common/tmpdir');
const fs = require('fs');
const path = require('path');

tmpdir.refresh();

// Should warn when trying to delete a nonexistent path
{
  common.expectWarning(
    'DeprecationWarning',
    'Permissive rmdir recursive is deprecated, use rm recursive and force \
instead',
    'DEP0147'
  );
  fs.rmdir(
    path.join(tmpdir.path, 'noexist.txt'),
    { recursive: true },
    common.mustCall()
  );
}
