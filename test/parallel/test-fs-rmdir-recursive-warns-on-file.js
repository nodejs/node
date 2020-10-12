// Flags: --expose-internals
'use strict';
const common = require('../common');
const tmpdir = require('../common/tmpdir');
const fs = require('fs');
const path = require('path');

tmpdir.refresh();

common.expectWarning(
  'DeprecationWarning',
  'Permissive rmdir recursive is deprecated, use rm recursive and force \
instead',
  'DEP0147'
);

{
  const filePath = path.join(tmpdir.path, 'rmdir-recursive.txt');
  fs.writeFileSync(filePath, '');

  // Should warn when trying to delete a file
  fs.rmdirSync(filePath, { recursive: true });
}
