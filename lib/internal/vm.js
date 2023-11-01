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
  vm_dynamic_import_missing_flag,
} = internalBinding('symbols');
const {
  validateFunction,
  validateObject,
  kValidateObjectAllowArray,
} = require('internal/validators');

const {
  getOptionValue,
} = require('internal/options');


function isContext(object) {
  validateObject(object, 'object', kValidateObjectAllowArray);

  return _isContext(object);
}

function getHostDefinedOptionId(importModuleDynamically, hint) {
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
  // We should've thrown here immediately when we introduced
  // --experimental-vm-modules and importModuleDynamically, but since
  // users are already using this callback to throw a similar error,
  // we also defer the error to the time when an actual import() is called
  // to avoid breaking them. To ensure that the isolate compilation
  // cache can still be hit, use a constant sentinel symbol here.
  if (!getOptionValue('--experimental-vm-modules')) {
    return vm_dynamic_import_missing_flag;
  }

  return Symbol(hint);
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
