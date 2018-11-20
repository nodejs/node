'use strict';

require('../common');
const assert = require('assert');
const { execFileSync } = require('child_process');

// This test checks that untrusted code mitigations in V8 are disabled
// by default.

const v8Options = execFileSync(process.execPath, ['--v8-options']).toString();

const untrustedFlag = v8Options.indexOf('--untrusted-code-mitigations');
assert.notStrictEqual(untrustedFlag, -1);

const nextFlag = v8Options.indexOf('--', untrustedFlag + 2);
const slice = v8Options.substring(untrustedFlag, nextFlag);

assert(slice.match(/type: bool  default: false/));
