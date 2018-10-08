// Flags: --expose-internals
'use strict';
const { internalBinding } = require('internal/test/binding');
const common = require('../common');
const os = require('os');

const { hasSmallICU } = internalBinding('config');
if (!(common.hasIntl && hasSmallICU))
  common.skip('missing Intl');

const assert = require('assert');
const { spawnSync } = require('child_process');

const expected =
    'could not initialize ICU (check NODE_ICU_DATA or ' +
    `--icu-data-dir parameters)${os.EOL}`;

{
  const child = spawnSync(process.execPath, ['--icu-data-dir=/', '-e', '0']);
  assert(child.stderr.toString().includes(expected));
}

{
  const env = Object.assign({}, process.env, { NODE_ICU_DATA: '/' });
  const child = spawnSync(process.execPath, ['-e', '0'], { env });
  assert(child.stderr.toString().includes(expected));
}
