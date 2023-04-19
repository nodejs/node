'use strict';

const {
  DateNow,
  NumberIsNaN,
  ObjectDefineProperties,
  ReflectConstruct,
  StringPrototypeToWellFormed,
  Symbol,
  SymbolToStringTag,
} = primordials;

const {
  Blob,
} = require('internal/blob');

const {
  customInspectSymbol: kInspect,
  kEnumerableProperty,
  kEmptyObject,
} = require('internal/util');

const {
  codes: {
    ERR_MISSING_ARGS,
    ERR_INVALID_THIS,
  },
} = require('internal/errors');

const {
  inspect,
} = require('internal/util/inspect');

const {
  makeTransferable,
  kClone,
  kDeserialize,
} = require('internal/worker/js_transferable');

const kState = Symbol('state');

function isFile(object) {
  return object?.[kState] !== undefined;
}

class File extends Blob {
  [kState] = { __proto__: null };

  constructor(fileBits, fileName, options = kEmptyObject) {
    if (arguments.length < 2) {
      throw new ERR_MISSING_ARGS('fileBits', 'fileName');
    }

    super(fileBits, options);

    let { lastModified } = options ?? kEmptyObject;

    if (lastModified !== undefined) {
      // Using Number(...) will not throw an error for bigints.
      lastModified = +lastModified;

      if (NumberIsNaN(lastModified)) {
        lastModified = 0;
      }
    } else {
      lastModified = DateNow();
    }

    this[kState].name = StringPrototypeToWellFormed(`${fileName}`);
    this[kState].lastModified = lastModified;

    makeTransferable(this);
  }

  get name() {
    if (!isFile(this))
      throw new ERR_INVALID_THIS('File');

    return this[kState].name;
  }

  get lastModified() {
    if (!isFile(this))
      throw new ERR_INVALID_THIS('File');

    return this[kState].lastModified;
  }

  [kInspect](depth, options) {
    if (depth < 0) {
      return this;
    }

    const opts = {
      ...options,
      depth: options.depth == null ? null : options.depth - 1,
    };

    return `File ${inspect({
      size: this.size,
      type: this.type,
      name: this[kState].name,
      lastModified: this[kState].lastModified,
    }, opts)}`;
  }

  [kClone]() {
    return {
      data: { ...super[kClone]().data, ...this[kState] },
      deserializeInfo: 'internal/file:ClonedFile',
    };
  }

  [kDeserialize](data) {
    super[kDeserialize](data);

    this[kState] = {
      __proto__: null,
      name: data.name,
      lastModified: data.lastModified,
    };
  }
}

function ClonedFile() {
  return makeTransferable(ReflectConstruct(function() {}, [], File));
}
ClonedFile.prototype[kDeserialize] = () => {};

ObjectDefineProperties(File.prototype, {
  name: kEnumerableProperty,
  lastModified: kEnumerableProperty,
  [SymbolToStringTag]: {
    __proto__: null,
    configurable: true,
    value: 'File',
  },
});

module.exports = {
  File,
  ClonedFile,
};
