'use strict';

const {
  ObjectKeys,
} = primordials;

let session;
function sendInspectorCommand(cb, onError) {
  const { hasInspector } = internalBinding('config');
  if (!hasInspector) return onError();
  const inspector = require('inspector');
  if (session === undefined) session = new inspector.Session();
  session.connect();
  try {
    return cb(session);
  } finally {
    session.disconnect();
  }
}

// Create a special require function for the inspector command line API
function installConsoleExtensions(commandLineApi) {
  if (commandLineApi.require) { return; }
  const { tryGetCwd } = require('internal/process/execution');
  const CJSModule = require('internal/modules/cjs/loader').Module;
  const { makeRequireFunction } = require('internal/modules/cjs/helpers');
  const consoleAPIModule = new CJSModule('<inspector console>');
  const cwd = tryGetCwd();
  consoleAPIModule.paths =
      CJSModule._nodeModulePaths(cwd).concat(CJSModule.globalPaths);
  commandLineApi.require = makeRequireFunction(consoleAPIModule);
}

// Wrap a console implemented by Node.js with features from the VM inspector
function wrapConsole(consoleFromNode, consoleFromVM) {
  const { consoleCall } = internalBinding('inspector');
  for (const key of ObjectKeys(consoleFromVM)) {
    // If global console has the same method as inspector console,
    // then wrap these two methods into one. Native wrapper will preserve
    // the original stack.
    if (consoleFromNode.hasOwnProperty(key)) {
      consoleFromNode[key] = consoleCall.bind(consoleFromNode,
                                              consoleFromVM[key],
                                              consoleFromNode[key]);
    } else {
      // Add additional console APIs from the inspector
      consoleFromNode[key] = consoleFromVM[key];
    }
  }
}

// Stores the console from VM, should be set during bootstrap.
let consoleFromVM;
module.exports = {
  installConsoleExtensions,
  sendInspectorCommand,
  wrapConsole,
  get consoleFromVM() {
    return consoleFromVM;
  },
  set consoleFromVM(val) {
    consoleFromVM = val;
  }
};
