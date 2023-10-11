'use strict';

const {
  ReflectApply,
  Symbol,
} = primordials;

const {
  ContextifyScript,
  compileFunction,
  isContext: _isContext,
} = internalBinding('contextify');
const {
  runInContext,
} = ContextifyScript.prototype;
const {
  default_host_defined_options,
} = internalBinding('symbols');
const {
  validateFunction,
  validateObject,
  kValidateObjectAllowArray,
} = require('internal/validators');

function isContext(object) {
  validateObject(object, 'object', kValidateObjectAllowArray);

  return _isContext(object);
}

function getHostDefinedOptionId(importModuleDynamically, filename) {
  if (importModuleDynamically !== undefined) {
    // Check that it's either undefined or a function before we pass
    // it into the native constructor.
    validateFunction(importModuleDynamically,
                     'options.importModuleDynamically');
  }
  if (importModuleDynamically === undefined) {
    // We need a default host defined options that are the same for all
    // scripts not needing custom module callbacks so that the isolate
    // compilation cache can be hit.
    return default_host_defined_options;
  }
  return Symbol(filename);
}

function registerImportModuleDynamically(referrer, importModuleDynamically) {
  const { importModuleDynamicallyWrap } = require('internal/vm/module');
  const { registerModule } = require('internal/modules/esm/utils');
  registerModule(referrer, {
    __proto__: null,
    importModuleDynamically:
      importModuleDynamicallyWrap(importModuleDynamically),
  });
}

function internalCompileFunction(
  code, filename, lineOffset, columnOffset,
  cachedData, produceCachedData, parsingContext, contextExtensions,
  params, hostDefinedOptionId, importModuleDynamically) {
  const result = compileFunction(
    code,
    filename,
    lineOffset,
    columnOffset,
    cachedData,
    produceCachedData,
    parsingContext,
    contextExtensions,
    params,
    hostDefinedOptionId,
  );

  if (produceCachedData) {
    result.function.cachedDataProduced = result.cachedDataProduced;
  }

  if (result.cachedData) {
    result.function.cachedData = result.cachedData;
  }

  if (typeof result.cachedDataRejected === 'boolean') {
    result.function.cachedDataRejected = result.cachedDataRejected;
  }

  if (importModuleDynamically !== undefined) {
    registerImportModuleDynamically(result.function, importModuleDynamically);
  }

  return result;
}

function makeContextifyScript(code,
                              filename,
                              lineOffset,
                              columnOffset,
                              cachedData,
                              produceCachedData,
                              parsingContext,
                              hostDefinedOptionId,
                              importModuleDynamically) {
  let script;
  // Calling `ReThrow()` on a native TryCatch does not generate a new
  // abort-on-uncaught-exception check. A dummy try/catch in JS land
  // protects against that.
  try { // eslint-disable-line no-useless-catch
    script = new ContextifyScript(code,
                                  filename,
                                  lineOffset,
                                  columnOffset,
                                  cachedData,
                                  produceCachedData,
                                  parsingContext,
                                  hostDefinedOptionId);
  } catch (e) {
    throw e; /* node-do-not-add-exception-line */
  }

  if (importModuleDynamically !== undefined) {
    registerImportModuleDynamically(script, importModuleDynamically);
  }
  return script;
}

// Internal version of vm.Script.prototype.runInThisContext() which skips
// argument validation.
function runScriptInThisContext(script, displayErrors, breakOnFirstLine) {
  return ReflectApply(
    runInContext,
    script,
    [
      null,                // sandbox - use current context
      -1,                  // timeout
      displayErrors,       // displayErrors
      false,               // breakOnSigint
      breakOnFirstLine,    // breakOnFirstLine
    ],
  );
}

module.exports = {
  getHostDefinedOptionId,
  internalCompileFunction,
  isContext,
  makeContextifyScript,
  registerImportModuleDynamically,
  runScriptInThisContext,
};
