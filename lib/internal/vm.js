'use strict';

const {
  ArrayPrototypeForEach,
  Symbol,
} = primordials;

const {
  compileFunction,
  isContext: _isContext,
} = internalBinding('contextify');
const {
  default_host_defined_options,
} = internalBinding('symbols');
const {
  validateArray,
  validateBoolean,
  validateBuffer,
  validateFunction,
  validateObject,
  validateString,
  validateStringArray,
  validateInt32,
} = require('internal/validators');
const {
  ERR_INVALID_ARG_TYPE,
} = require('internal/errors').codes;

function isContext(object) {
  validateObject(object, 'object', { __proto__: null, allowArray: true });

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

function internalCompileFunction(code, params, options) {
  validateString(code, 'code');
  if (params !== undefined) {
    validateStringArray(params, 'params');
  }
  const {
    filename = '',
    columnOffset = 0,
    lineOffset = 0,
    cachedData = undefined,
    produceCachedData = false,
    parsingContext = undefined,
    contextExtensions = [],
    importModuleDynamically,
  } = options;

  validateString(filename, 'options.filename');
  validateInt32(columnOffset, 'options.columnOffset');
  validateInt32(lineOffset, 'options.lineOffset');
  if (cachedData !== undefined)
    validateBuffer(cachedData, 'options.cachedData');
  validateBoolean(produceCachedData, 'options.produceCachedData');
  if (parsingContext !== undefined) {
    if (
      typeof parsingContext !== 'object' ||
      parsingContext === null ||
      !isContext(parsingContext)
    ) {
      throw new ERR_INVALID_ARG_TYPE(
        'options.parsingContext',
        'Context',
        parsingContext,
      );
    }
  }
  validateArray(contextExtensions, 'options.contextExtensions');
  ArrayPrototypeForEach(contextExtensions, (extension, i) => {
    const name = `options.contextExtensions[${i}]`;
    validateObject(extension, name, { __proto__: null, nullable: true });
  });

  const hostDefinedOptionId =
      getHostDefinedOptionId(importModuleDynamically, filename);
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
    validateFunction(importModuleDynamically,
                     'options.importModuleDynamically');
    const { importModuleDynamicallyWrap } = require('internal/vm/module');
    const wrapped = importModuleDynamicallyWrap(importModuleDynamically);
    const func = result.function;
    const { registerModule } = require('internal/modules/esm/utils');
    registerModule(func, {
      __proto__: null,
      importModuleDynamically: wrapped,
    });
  }

  return result;
}

module.exports = {
  getHostDefinedOptionId,
  internalCompileFunction,
  isContext,
};
