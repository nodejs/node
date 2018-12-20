'use strict';
require('../common');
const assert = require('assert');
const { execFileSync } = require('child_process');

const entryPoints = ['iDoNotExist', 'iDoNotExist.js', 'iDoNotExist.mjs'];
const flags = [[], ['--preserve-symlinks']];
const node = process.argv[0];

for (const args of flags) {
  for (const entryPoint of entryPoints) {
    try {
      execFileSync(node, args.concat(entryPoint));
    } catch (e) {
      assert(e.toString().match(/Error: Cannot find module/));
      continue;
    }
    assert.fail('Executing node with inexistent entry point should ' +
                `fail. Entry point: ${entryPoint}, Flags: [${args}]`);
  }
}
