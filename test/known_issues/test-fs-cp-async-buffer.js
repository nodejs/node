'use strict';

// We expect this test to fail because the implementation of fsPromise.cp
// does not properly support the use of Buffer as the source or destination
// argument like fs.cpSync does.
// Refs: https://github.com/nodejs/node/issues/58634
// Refs: https://github.com/nodejs/node/issues/58869

const common = require('../common');
const { mkdirSync, promises } = require('fs');
const { join } = require('path');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();

const tmpA = join(tmpdir.path, 'a');
const tmpB = join(tmpdir.path, 'b');

mkdirSync(tmpA, { recursive: true });

promises.cp(Buffer.from(tmpA), Buffer.from(tmpB), {
  recursive: true,
}).then(common.mustCall());
