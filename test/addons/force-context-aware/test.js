'use strict';
const common = require('../../common');
const childProcess = require('child_process');
const assert = require('assert');
const path = require('path');

const mod = path.join('test', 'addons', 'force-context-aware', 'index.js');

const execString = `"${process.execPath}" --force-context-aware ./${mod}`;
childProcess.exec(execString, common.mustCall((err) => {
  const errMsg = 'Loading non context-aware native modules has been disabled';
  assert.strictEqual(err.message.includes(errMsg), true);
}));
