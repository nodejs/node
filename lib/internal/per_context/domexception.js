'use strict';

const {
  ErrorCaptureStackTrace,
  ErrorPrototype,
  ObjectDefineProperties,
  ObjectDefineProperty,
  ObjectSetPrototypeOf,
  SafeMap,
  SafeSet,
  SafeWeakMap,
  SymbolToStringTag,
  TypeError,
} = primordials;

const kClone = 'nodejs.worker_threads.clone';
const kDeserialize = 'nodejs.worker_threads.deserialize';

function throwInvalidThisError(Base, type) {
  const err = new Base();
  const key = 'ERR_INVALID_THIS';
  ObjectDefineProperties(err, {
    message: {
      value: `Value of "this" must be of ${type}`,
      enumerable: false,
      writable: true,
      configurable: true,
    },
    toString: {
      value() {
        return `${this.name} [${key}]: ${this.message}`;
      },
      enumerable: false,
      writable: true,
      configurable: true,
    },
  });
  err.code = key;
  throw err;
}

const internalsMap = new SafeWeakMap();
const nameToCodeMap = new SafeMap();

// These were removed from the error names table.
// See https://github.com/heycam/webidl/pull/946.
const disusedNamesSet = new SafeSet()
  .add('DOMStringSizeError')
  .add('NoDataAllowedError')
  .add('ValidationError');

class DOMException extends JSTransferable {
  constructor(message = '', name = 'Error') {
    super();

    let code;
    if (disusedNamesSet.has(name)) {
      code = 0;
    } else {
      code = nameToCodeMap.get(name);
      code = code === undefined ? 0 : code;
    }

    const o = { __proto__: null };
    ErrorCaptureStackTrace(o);
    this[kDeserialize]({ name, message, code, stack: o.stack });
  }

  get name() {
    const internals = internalsMap.get(this);
    if (internals === undefined) {
      throwInvalidThisError(TypeError, 'DOMException');
    }
    return internals.name;
  }

  get message() {
    const internals = internalsMap.get(this);
    if (internals === undefined) {
      throwInvalidThisError(TypeError, 'DOMException');
    }
    return internals.message;
  }

  get code() {
    const internals = internalsMap.get(this);
    if (internals === undefined) {
      throwInvalidThisError(TypeError, 'DOMException');
    }
    return internals.code;
  }

  get stack() {
    const internals = internalsMap.get(this);
    if (internals === undefined) {
      throwInvalidThisError(TypeError, 'DOMException');
    }
    return internals.stack;
  }

  [kClone]() {
    const { name, message, code, stack } = this;
    return {
      data: { name, message, code, stack },
      deserializeInfo: 'internal/per_context/domexception:DOMException',
    };
  }

  [kDeserialize]({ name, message, code, stack }) {
    internalsMap.set(this, {
      name: `${name}`,
      message: `${message}`,
      code,
      stack: `${stack}`,
    });
  }
}

ObjectSetPrototypeOf(DOMException.prototype, ErrorPrototype);
ObjectDefineProperties(DOMException.prototype, {
  [SymbolToStringTag]: { configurable: true, value: 'DOMException' },
  name: { enumerable: true, configurable: true },
  message: { enumerable: true, configurable: true },
  code: { enumerable: true, configurable: true },
  stack: { enumerable: true, configurable: true },
});

for (const { 0: name, 1: codeName, 2: value } of [
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
  ['DataCloneError', 'DATA_CLONE_ERR', 25],
  // There are some more error names, but since they don't have codes assigned,
  // we don't need to care about them.
]) {
  const desc = { enumerable: true, value };
  ObjectDefineProperty(DOMException, codeName, desc);
  ObjectDefineProperty(DOMException.prototype, codeName, desc);
  nameToCodeMap.set(name, value);
}

exports.DOMException = DOMException;
