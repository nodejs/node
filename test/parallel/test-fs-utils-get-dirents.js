// Flags: --expose-internals
'use strict';

const { getDirent, getDirents } = require('internal/fs/utils');
const assert = require('assert');
const { internalBinding } = require('internal/test/binding');
const { UV_DIRENT_UNKNOWN } = internalBinding('constants').fs;
const common = require('../common');
const fs = require('fs');
const path = require('path');

const tmpdir = require('../common/tmpdir');
const filename = 'foo';

{
    // setup
    tmpdir.refresh();
    fs.writeFileSync(path.join(tmpdir.path, filename), '')
}
{
    // string + string
    getDirents(
        tmpdir.path,
        [[filename], [UV_DIRENT_UNKNOWN]],
        common.mustCall((err, names) => {
            assert.strictEqual(err, null);
            assert.strictEqual(names.length, 1);
        },
    ));
}
{
    // Buffer + string
    getDirents(
        Buffer.from(tmpdir.path),
        [[filename], [UV_DIRENT_UNKNOWN]],
        common.mustCall((err, names) => {
            assert.strictEqual(err, null);
            assert.strictEqual(names.length, 1);
        },
    ));
}