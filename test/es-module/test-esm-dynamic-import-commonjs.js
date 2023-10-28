'use strict';

const common = require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('node:assert');
const fs = require('node:fs/promises');

tmpdir.refresh();

const preloadURL = tmpdir.fileURL('preload.cjs');
const commonjsURL = tmpdir.fileURL('commonjs-module-1.cjs');

(async () => {

    await Promise.all([fs.writeFile(preloadURL, ''), fs.writeFile(commonjsURL, '')]);

    // Make sure that the CommonJS module lexer has been initialized.
    // See https://github.com/nodejs/node/blob/v21.1.0/lib/internal/modules/esm/translators.js#L61-L81.
    await import(preloadURL);

    let tickDuringCJSImport = false;
    process.nextTick(() => { tickDuringCJSImport = true; });
    await import(commonjsURL);
    
    assert(!tickDuringCJSImport);

})().then(common.mustCall());
