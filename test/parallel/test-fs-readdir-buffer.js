'use strict';
const fs = require('fs');

const common = require('../common');
if (common.isWindows) {
    common.skip('windows doesnt support /dev');
}

const assert = require('assert');

fs.readdir(
    Buffer.from("/dev"),
    {withFileTypes: true, encoding: "buffer"},
    common.mustCall((e,d) => {
        assert.strictEqual(e, null);
    })
);
