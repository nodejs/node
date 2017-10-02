'use strict';
const common = require('../common');
const assert = require('assert');
const child_process = require('child_process');
const { promisify } = require('util');
const execFile = promisify(child_process.execFile);

common.crashOnUnhandledRejection();

const entryPoints = ['iDoNotExist', 'iDoNotExist.js', 'iDoNotExist.mjs'];
const flags = [[], ['--experimental-modules']];
const node = process.argv[0];

(async () => {
  for (const args of flags) {
    for (const entryPoint of entryPoints) {
      try {
        await execFile(node, args.concat(entryPoint));
      } catch (e) {
        assert.strictEqual(e.stdout, '');
        assert(e.stderr.match(/Error: Cannot find module/));
        continue;
      }
      throw new Error('Executing node with inexistent entry point should ' +
                      `fail. Entry point: ${entryPoint}, Flags: [${args}]`);
    }
  }
})();
