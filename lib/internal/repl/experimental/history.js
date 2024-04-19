'use strict';

const {
  PromisePrototypeThen,
  StringPrototypeSplit,
} = primordials;

const { exports: fs } = require('internal/fs/promises');
const permission = require('internal/process/permission');
const { join } = require('path');
const { homedir, EOL: osEOL } = require('os');

module.exports = async () => {
  let handle;

  try {
    const historyPath = join(homedir(), '.node_repl_history');
    if (permission.isEnabled() && permission.has('fs.write', historyPath) === false) {
      process.stdout.write('\nAccess to FileSystemWrite is restricted.\n' +
        'REPL session history will not be persisted.\n');
      return {
        __proto__: null,
        history: [],
        writeHistory: () => false,
      };
    }

    handle = await fs.open(historyPath, 'a+', 0o0600);
    const data = await handle.readFile({ encoding: 'utf8' });
    const history = StringPrototypeSplit(data, osEOL, 1000);
    const writeHistory = async (d) => {
      if (!handle) {
        return false;
      }
      try {
        await handle.truncate(0);
        await handle.writeFile(d.join(osEOL));
        return true;
      } catch {
        PromisePrototypeThen(handle.close(), undefined, () => {});
        handle = null;
        return false;
      }
    };
    return { __proto__: null, history, writeHistory, closeHandle: () => handle.close() };
  } catch {
    if (handle) {
      PromisePrototypeThen(handle.close(), undefined, () => {});
    }
    return { __proto__: null, history: [], writeHistory: () => false, closeHandle: () => undefined };
  }
};
