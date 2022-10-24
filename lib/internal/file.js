'use strict';

const {
  DateNow,
  NumberIsNaN,
  ObjectDefineProperties,
  ObjectPrototypeHasOwnProperty,
  Symbol,
  SymbolToStringTag,
} = primordials;

const {
  Blob,
} = require('internal/blob');

const {
  customInspectSymbol: kInspect,
  emitExperimentalWarning,
  kEnumerableProperty,
  kEmptyObject,
  toUSVString,
} = require('internal/util');

const {
  codes: {
    ERR_INVALID_THIS,
    ERR_MISSING_ARGS,
  },
} = require('internal/errors');

const {
  inspect,
} = require('internal/util/inspect');

const kState = Symbol('kState');

function isFile(object) {
  return object?.[kState] !== undefined || object instanceof Blob;
}

class File extends Blob {
  constructor(fileBits, fileName, options = kEmptyObject) {
    emitExperimentalWarning('buffer.File');

    if (arguments.length < 2) {
      throw new ERR_MISSING_ARGS('fileBits', 'fileName');
    }

    super(fileBits, options);

    let lastModified;

    if (ObjectPrototypeHasOwnProperty(options, 'lastModified')) {
      // Using Number(...) will not throw an error for bigints.
      lastModified = +options.lastModified;

      if (NumberIsNaN(lastModified)) {
        lastModified = 0;
      }
    } else {
      lastModified = DateNow();
    }

    this[kState] = {
      name: toUSVString(fileName),
      lastModified,
    };
  }

  get name() {
    if (!isFile(this)) {
      throw new ERR_INVALID_THIS('File');
    }

    return this[kState].name;
  }

  get lastModified() {
    if (!isFile(this)) {
      throw new ERR_INVALID_THIS('File');
    }

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
      name: this.name,
      lastModified: this.lastModified,
    }, opts)}`;
  }
}

ObjectDefineProperties(File.prototype, {
  name: kEnumerableProperty,
  lastModified: kEnumerableProperty,
  [SymbolToStringTag]: {
    __proto__: null,
    configurable: true,
    value: 'File',
  }
});

module.exports = {
  File,
};
