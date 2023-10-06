'use strict';

const {
  ArrayPrototypeForEach,
} = primordials;

const {
  compileFunction,
  isContext: _isContext,
} = internalBinding('contextify');
const {
  validateArray,
  validateBoolean,
  validateBuffer,
  validateFunction,
  validateObject,
  validateString,
  validateStringArray,
  kValidateObjectAllowArray,
  kValidateObjectAllowNullable,
  validateInt32,
} = require('internal/validators');
const {
  ERR_INVALID_ARG_TYPE,
} = require('internal/errors').codes;

function isContext(object) {
  validateObject(object, 'object', kValidateObjectAllowArray);

  return _isContext(object);
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
    validateObject(extension, name, kValidateObjectAllowNullable);
  });

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
  internalCompileFunction,
  isContext,
};
