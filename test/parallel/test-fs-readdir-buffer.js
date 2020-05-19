'use strict';
const fs = require('fs');

const common = require('../common');
if (!common.isOSX) {
    common.skip('this tests works only on MacOS');
}

const assert = require('assert');

fs.readdir(
    Buffer.from("/dev"),
    {withFileTypes: true, encoding: "buffer"},
    common.mustCall((e,d) => {
        assert.strictEqual(e, null);
    })
);
