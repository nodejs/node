'use strict';
const common = require('../common');
const { spawnSync } = require('child_process');
const stateful = {
  toString: common.mustCall(() => ';'),
};
spawnSync(process.execPath, ['-e', stateful], { stdio: 'ignore' });
