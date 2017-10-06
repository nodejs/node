'use strict';

const { URL } = require('url');
const CJSmodule = require('module');
const errors = require('internal/errors');
const { resolve } = process.binding('module_wrap');

module.exports = (target, base) => {
  target = `${target}`;
  if (base === undefined) {
    // We cannot search without a base.
    throw new errors.Error('ERR_MISSING_MODULE', target);
  }
  base = `${base}`;
  try {
    return resolve(target, base);
  } catch (e) {
    e.stack; // cause V8 to generate stack before rethrow
    let error = e;
    try {
      const questionedBase = new URL(base);
      const tmpMod = new CJSmodule(questionedBase.pathname, null);
      tmpMod.paths = CJSmodule._nodeModulePaths(
        new URL('./', questionedBase).pathname);
      const found = CJSmodule._resolveFilename(target, tmpMod);
      error = new errors.Error('ERR_MODULE_RESOLUTION_LEGACY', target,
                               base, found);
    } catch (problemChecking) {
      // ignore
    }
    throw error;
  }
};
