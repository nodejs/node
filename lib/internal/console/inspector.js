'use strict';

const path = require('path');
const CJSModule = require('internal/modules/cjs/loader');
const { makeRequireFunction } = require('internal/modules/cjs/helpers');
const { tryGetCwd } = require('internal/util');
const { addCommandLineAPI, consoleCall } = process.binding('inspector');

// Wrap a console implemented by Node.js with features from the VM inspector
function addInspectorApis(consoleFromNode, consoleFromVM) {
  // Setup inspector command line API.
  const cwd = tryGetCwd(path);
  const consoleAPIModule = new CJSModule('<inspector console>');
  consoleAPIModule.paths =
      CJSModule._nodeModulePaths(cwd).concat(CJSModule.globalPaths);
  addCommandLineAPI('require', makeRequireFunction(consoleAPIModule));
  const config = {};

  // If global console has the same method as inspector console,
  // then wrap these two methods into one. Native wrapper will preserve
  // the original stack.
  for (const key of Object.keys(consoleFromNode)) {
    if (!consoleFromVM.hasOwnProperty(key))
      continue;
    consoleFromNode[key] = consoleCall.bind(consoleFromNode,
                                            consoleFromVM[key],
                                            consoleFromNode[key],
                                            config);
  }

  // Add additional console APIs from the inspector
  for (const key of Object.keys(consoleFromVM)) {
    if (consoleFromNode.hasOwnProperty(key))
      continue;
    consoleFromNode[key] = consoleFromVM[key];
  }
}

module.exports = {
  addInspectorApis
};

// Stores the console from VM, should be set during bootstrap.
let consoleFromVM;

Object.defineProperty(module.exports, 'consoleFromVM', {
  get() {
    return consoleFromVM;
  },
  set(val) {
    consoleFromVM = val;
  }
});
