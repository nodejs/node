
'use strict';
const common = require('../common');
const assert = require('assert');
const child = require('child_process');

// Regression test for https://github.com/nodejs/node/issues/60808
// verify that util.parseArgs works correctly with --eval=...

if (process.argv[2] === 'child') {
    const { parseArgs } = require('util');
    const { positionals } = parseArgs({ allowPositionals: true });
    console.log(JSON.stringify(positionals));
    return;
}

{
    const code = 'console.log(JSON.stringify(require("util").parseArgs({allowPositionals:true}).positionals))';

    // Test with --eval=... syntax
    child.execFile(process.execPath, [
        `--eval=${code}`,
        '--',
        'arg1',
        'arg2'
    ], common.mustSucceed((stdout) => {
        assert.strictEqual(stdout.trim(), '["arg1","arg2"]');
    }));


    // Test with --print=... syntax
    const codePrint = 'JSON.stringify(require("util").parseArgs({allowPositionals:true}).positionals)';
    child.execFile(process.execPath, [
        `--print=${codePrint}`,
        '--',
        'arg1',
        'arg2'
    ], common.mustSucceed((stdout) => {
        assert.strictEqual(stdout.trim(), '["arg1","arg2"]');
    }));
}
