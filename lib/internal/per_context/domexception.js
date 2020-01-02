'use strict';

const {
  Error,
  ObjectDefineProperties,
  ObjectDefineProperty,
  SafeWeakMap,
  SafeMap,
  SymbolToStringTag,
} = primordials;

class ERR_INVALID_THIS extends TypeError {
  constructor(type) {
    super('Value of "this" must be of ' + type);
  }

  get code() { return 'ERR_INVALID_THIS'; }
}

let internalsMap;
let nameToCodeMap;
let isInitialized = false;

// We need to instantiate the maps lazily because they render
// the snapshot non-rehashable.
// https://bugs.chromium.org/p/v8/issues/detail?id=6593
function ensureInitialized() {
  if (isInitialized) {
    return;
  }
  internalsMap = new SafeWeakMap();
  nameToCodeMap = new SafeMap();
  forEachCode((name, codeName, value) => {
    nameToCodeMap.set(name, value);
  });
  isInitialized = true;
}

class DOMException extends Error {
  constructor(message = '', name = 'Error') {
    ensureInitialized();
    super();
    internalsMap.set(this, {
      message: `${message}`,
      name: `${name}`
    });
  }

  get name() {
    ensureInitialized();
    const internals = internalsMap.get(this);
    if (internals === undefined) {
      throw new ERR_INVALID_THIS('DOMException');
    }
    return internals.name;
  }

  get message() {
    ensureInitialized();
    const internals = internalsMap.get(this);
    if (internals === undefined) {
      throw new ERR_INVALID_THIS('DOMException');
    }
    return internals.message;
  }

  get code() {
    ensureInitialized();
    const internals = internalsMap.get(this);
    if (internals === undefined) {
      throw new ERR_INVALID_THIS('DOMException');
    }
    const code = nameToCodeMap.get(internals.name);
    return code === undefined ? 0 : code;
  }
}

ObjectDefineProperties(DOMException.prototype, {
  [SymbolToStringTag]: { configurable: true, value: 'DOMException' },
  name: { enumerable: true, configurable: true },
  message: { enumerable: true, configurable: true },
  code: { enumerable: true, configurable: true }
});

function forEachCode(fn) {
  fn('IndexSizeError', 'INDEX_SIZE_ERR', 1);
  fn('DOMStringSizeError', 'DOMSTRING_SIZE_ERR', 2);
  fn('HierarchyRequestError', 'HIERARCHY_REQUEST_ERR', 3);
  fn('WrongDocumentError', 'WRONG_DOCUMENT_ERR', 4);
  fn('InvalidCharacterError', 'INVALID_CHARACTER_ERR', 5);
  fn('NoDataAllowedError', 'NO_DATA_ALLOWED_ERR', 6);
  fn('NoModificationAllowedError', 'NO_MODIFICATION_ALLOWED_ERR', 7);
  fn('NotFoundError', 'NOT_FOUND_ERR', 8);
  fn('NotSupportedError', 'NOT_SUPPORTED_ERR', 9);
  fn('InUseAttributeError', 'INUSE_ATTRIBUTE_ERR', 10);
  fn('InvalidStateError', 'INVALID_STATE_ERR', 11);
  fn('SyntaxError', 'SYNTAX_ERR', 12);
  fn('InvalidModificationError', 'INVALID_MODIFICATION_ERR', 13);
  fn('NamespaceError', 'NAMESPACE_ERR', 14);
  fn('InvalidAccessError', 'INVALID_ACCESS_ERR', 15);
  fn('ValidationError', 'VALIDATION_ERR', 16);
  fn('TypeMismatchError', 'TYPE_MISMATCH_ERR', 17);
  fn('SecurityError', 'SECURITY_ERR', 18);
  fn('NetworkError', 'NETWORK_ERR', 19);
  fn('AbortError', 'ABORT_ERR', 20);
  fn('URLMismatchError', 'URL_MISMATCH_ERR', 21);
  fn('QuotaExceededError', 'QUOTA_EXCEEDED_ERR', 22);
  fn('TimeoutError', 'TIMEOUT_ERR', 23);
  fn('InvalidNodeTypeError', 'INVALID_NODE_TYPE_ERR', 24);
  fn('DataCloneError', 'DATA_CLONE_ERR', 25);
  // There are some more error names, but since they don't have codes assigned,
  // we don't need to care about them.
}

forEachCode((name, codeName, value) => {
  const desc = { enumerable: true, value };
  ObjectDefineProperty(DOMException, codeName, desc);
  ObjectDefineProperty(DOMException.prototype, codeName, desc);
});

exports.DOMException = DOMException;
