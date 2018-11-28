'use strict';

const { ERR_INVALID_THIS } = require('internal/errors').codes;

const internalsMap = new WeakMap();

const nameToCodeMap = new Map();

class DOMException extends Error {
  constructor(message = '', name = 'Error') {
    super();
    internalsMap.set(this, {
      message: `${message}`,
      name: `${name}`
    });
  }

  get name() {
    const internals = internalsMap.get(this);
    if (internals === undefined) {
      throw new ERR_INVALID_THIS('DOMException');
    }
    return internals.name;
  }

  get message() {
    const internals = internalsMap.get(this);
    if (internals === undefined) {
      throw new ERR_INVALID_THIS('DOMException');
    }
    return internals.message;
  }

  get code() {
    const internals = internalsMap.get(this);
    if (internals === undefined) {
      throw new ERR_INVALID_THIS('DOMException');
    }
    const code = nameToCodeMap.get(internals.name);
    return code === undefined ? 0 : code;
  }
}

Object.defineProperties(DOMException.prototype, {
  [Symbol.toStringTag]: { configurable: true, value: 'DOMException' },
  name: { enumerable: true, configurable: true },
  message: { enumerable: true, configurable: true },
  code: { enumerable: true, configurable: true }
});

for (const [name, codeName, value] of [
  ['IndexSizeError', 'INDEX_SIZE_ERR', 1],
  ['DOMStringSizeError', 'DOMSTRING_SIZE_ERR', 2],
  ['HierarchyRequestError', 'HIERARCHY_REQUEST_ERR', 3],
  ['WrongDocumentError', 'WRONG_DOCUMENT_ERR', 4],
  ['InvalidCharacterError', 'INVALID_CHARACTER_ERR', 5],
  ['NoDataAllowedError', 'NO_DATA_ALLOWED_ERR', 6],
  ['NoModificationAllowedError', 'NO_MODIFICATION_ALLOWED_ERR', 7],
  ['NotFoundError', 'NOT_FOUND_ERR', 8],
  ['NotSupportedError', 'NOT_SUPPORTED_ERR', 9],
  ['InUseAttributeError', 'INUSE_ATTRIBUTE_ERR', 10],
  ['InvalidStateError', 'INVALID_STATE_ERR', 11],
  ['SyntaxError', 'SYNTAX_ERR', 12],
  ['InvalidModificationError', 'INVALID_MODIFICATION_ERR', 13],
  ['NamespaceError', 'NAMESPACE_ERR', 14],
  ['InvalidAccessError', 'INVALID_ACCESS_ERR', 15],
  ['ValidationError', 'VALIDATION_ERR', 16],
  ['TypeMismatchError', 'TYPE_MISMATCH_ERR', 17],
  ['SecurityError', 'SECURITY_ERR', 18],
  ['NetworkError', 'NETWORK_ERR', 19],
  ['AbortError', 'ABORT_ERR', 20],
  ['URLMismatchError', 'URL_MISMATCH_ERR', 21],
  ['QuotaExceededError', 'QUOTA_EXCEEDED_ERR', 22],
  ['TimeoutError', 'TIMEOUT_ERR', 23],
  ['InvalidNodeTypeError', 'INVALID_NODE_TYPE_ERR', 24],
  ['DataCloneError', 'DATA_CLONE_ERR', 25]
  // There are some more error names, but since they don't have codes assigned,
  // we don't need to care about them.
]) {
  const desc = { enumerable: true, value };
  Object.defineProperty(DOMException, codeName, desc);
  Object.defineProperty(DOMException.prototype, codeName, desc);
  nameToCodeMap.set(name, value);
}

module.exports = DOMException;
