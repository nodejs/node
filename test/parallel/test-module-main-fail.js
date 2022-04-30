'use strict';
require('../common');
const assert = require('assert');
const { execFileSync } = require('child_process');

const entryPoints = ['iDoNotExist', 'iDoNotExist.js', 'iDoNotExist.mjs'];
const node = process.argv[0];

for (const entryPoint of entryPoints) {
  try {
    execFileSync(node, [entryPoint], { stdio: 'pipe' });
  } catch (e) {
    const error = e.toString();
    assert(error.match(/ERR_MODULE_NOT_FOUND/));
    assert(error.match(/Cannot find module/));
    assert(error.match(entryPoint));
    continue;
  }
  assert.fail('Executing node with inexistent entry point should ' +
              `fail. Entry point: ${entryPoint}`);
}
