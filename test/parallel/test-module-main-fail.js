'use strict';
require('../common');
const assert = require('assert');
const { execFile } = require('child_process');
const common = require('../common');

const entryPoints = ['iDoNotExist', 'iDoNotExist.js', 'iDoNotExist.mjs'];
const flags = [[], ['--experimental-modules']];
const node = process.argv[0];

for (const args of flags) {
  for (const entryPoint of entryPoints) {
    execFile(
      node,
      args.concat(entryPoint),
      common.mustCall((err, stdout, stderr) => {
        if (!stderr) {
          assert.fail('Executing node with inexistent entry point should ' +
                  `fail. Entry point: ${entryPoint}, Flags: [${args}]`);
        }
        assert(stderr.toString().match(/Error: Cannot find module/));
      })
    );
  }
}
