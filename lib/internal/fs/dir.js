'use strict';

const {
  ArrayPrototypePush,
  ArrayPrototypeSlice,
  ArrayPrototypeSplice,
  FunctionPrototypeBind,
  ObjectDefineProperty,
  PromiseReject,
  Symbol,
  SymbolAsyncIterator,
} = primordials;

const pathModule = require('path');
const binding = internalBinding('fs');
const dirBinding = internalBinding('fs_dir');
const {
  codes: {
    ERR_DIR_CLOSED,
    ERR_DIR_CONCURRENT_OPERATION,
    ERR_MISSING_ARGS
  }
} = require('internal/errors');

const { FSReqCallback } = binding;
const internalUtil = require('internal/util');
const {
  getDirent,
  getOptions,
  getValidatedPath,
  handleErrorFromBinding
} = require('internal/fs/utils');
const {
  validateFunction,
  validateUint32
} = require('internal/validators');

const kDirHandle = Symbol('kDirHandle');
const kDirPath = Symbol('kDirPath');
const kDirBufferedEntries = Symbol('kDirBufferedEntries');
const kDirClosed = Symbol('kDirClosed');
const kDirOptions = Symbol('kDirOptions');
const kDirReadImpl = Symbol('kDirReadImpl');
const kDirReadPromisified = Symbol('kDirReadPromisified');
const kDirClosePromisified = Symbol('kDirClosePromisified');
const kDirOperationQueue = Symbol('kDirOperationQueue');

class Dir {
  constructor(handle, path, options) {
    if (handle == null) throw new ERR_MISSING_ARGS('handle');
    this[kDirHandle] = handle;
    this[kDirBufferedEntries] = [];
    this[kDirPath] = path;
    this[kDirClosed] = false;

    // Either `null` or an Array of pending operations (= functions to be called
    // once the current operation is done).
    this[kDirOperationQueue] = null;

    this[kDirOptions] = {
      bufferSize: 32,
      ...getOptions(options, {
        encoding: 'utf8'
      })
    };

    validateUint32(this[kDirOptions].bufferSize, 'options.bufferSize', true);

    this[kDirReadPromisified] = FunctionPrototypeBind(
      internalUtil.promisify(this[kDirReadImpl]), this, false);
    this[kDirClosePromisified] = FunctionPrototypeBind(
      internalUtil.promisify(this.close), this);
  }

  get path() {
    return this[kDirPath];
  }

  read(callback) {
    return this[kDirReadImpl](true, callback);
  }

  [kDirReadImpl](maybeSync, callback) {
    if (this[kDirClosed] === true) {
      throw new ERR_DIR_CLOSED();
    }

    if (callback === undefined) {
      return this[kDirReadPromisified]();
    }

    validateFunction(callback, 'callback');

    if (this[kDirOperationQueue] !== null) {
      ArrayPrototypePush(this[kDirOperationQueue], () => {
        this[kDirReadImpl](maybeSync, callback);
      });
      return;
    }

    if (this[kDirBufferedEntries].length > 0) {
      const { 0: name, 1: type } =
        ArrayPrototypeSplice(this[kDirBufferedEntries], 0, 2);
      if (maybeSync)
        process.nextTick(getDirent, this[kDirPath], name, type, callback);
      else
        getDirent(this[kDirPath], name, type, callback);
      return;
    }

    const req = new FSReqCallback();
    req.oncomplete = (err, result) => {
      process.nextTick(() => {
        const queue = this[kDirOperationQueue];
        this[kDirOperationQueue] = null;
        for (const op of queue) op();
      });

      if (err || result === null) {
        return callback(err, result);
      }

      this[kDirBufferedEntries] = ArrayPrototypeSlice(result, 2);
      getDirent(this[kDirPath], result[0], result[1], callback);
    };

    this[kDirOperationQueue] = [];
    this[kDirHandle].read(
      this[kDirOptions].encoding,
      this[kDirOptions].bufferSize,
      req
    );
  }

  readSync() {
    if (this[kDirClosed] === true) {
      throw new ERR_DIR_CLOSED();
    }

    if (this[kDirOperationQueue] !== null) {
      throw new ERR_DIR_CONCURRENT_OPERATION();
    }

    if (this[kDirBufferedEntries].length > 0) {
      const { 0: name, 1: type } =
          ArrayPrototypeSplice(this[kDirBufferedEntries], 0, 2);
      return getDirent(this[kDirPath], name, type);
    }

    const ctx = { path: this[kDirPath] };
    const result = this[kDirHandle].read(
      this[kDirOptions].encoding,
      this[kDirOptions].bufferSize,
      undefined,
      ctx
    );
    handleErrorFromBinding(ctx);

    if (result === null) {
      return result;
    }

    this[kDirBufferedEntries] = ArrayPrototypeSlice(result, 2);
    return getDirent(this[kDirPath], result[0], result[1]);
  }

  close(callback) {
    // Promise
    if (callback === undefined) {
      if (this[kDirClosed] === true) {
        return PromiseReject(new ERR_DIR_CLOSED());
      }
      return this[kDirClosePromisified]();
    }

    // callback
    validateFunction(callback, 'callback');

    if (this[kDirClosed] === true) {
      process.nextTick(callback, new ERR_DIR_CLOSED());
      return;
    }

    if (this[kDirOperationQueue] !== null) {
      ArrayPrototypePush(this[kDirOperationQueue], () => {
        this.close(callback);
      });
      return;
    }

    this[kDirClosed] = true;
    const req = new FSReqCallback();
    req.oncomplete = callback;
    this[kDirHandle].close(req);
  }

  closeSync() {
    if (this[kDirClosed] === true) {
      throw new ERR_DIR_CLOSED();
    }

    if (this[kDirOperationQueue] !== null) {
      throw new ERR_DIR_CONCURRENT_OPERATION();
    }

    this[kDirClosed] = true;
    const ctx = { path: this[kDirPath] };
    const result = this[kDirHandle].close(undefined, ctx);
    handleErrorFromBinding(ctx);
    return result;
  }

  async* entries() {
    try {
      while (true) {
        const result = await this[kDirReadPromisified]();
        if (result === null) {
          break;
        }
        yield result;
      }
    } finally {
      await this[kDirClosePromisified]();
    }
  }
}

ObjectDefineProperty(Dir.prototype, SymbolAsyncIterator, {
  __proto__: null,
  value: Dir.prototype.entries,
  enumerable: false,
  writable: true,
  configurable: true,
});

function opendir(path, options, callback) {
  callback = typeof options === 'function' ? options : callback;
  validateFunction(callback, 'callback');

  path = getValidatedPath(path);
  options = getOptions(options, {
    encoding: 'utf8'
  });

  function opendirCallback(error, handle) {
    if (error) {
      callback(error);
    } else {
      callback(null, new Dir(handle, path, options));
    }
  }

  const req = new FSReqCallback();
  req.oncomplete = opendirCallback;

  dirBinding.opendir(
    pathModule.toNamespacedPath(path),
    options.encoding,
    req
  );
}

function opendirSync(path, options) {
  path = getValidatedPath(path);
  options = getOptions(options, {
    encoding: 'utf8'
  });

  const ctx = { path };
  const handle = dirBinding.opendir(
    pathModule.toNamespacedPath(path),
    options.encoding,
    undefined,
    ctx
  );
  handleErrorFromBinding(ctx);

  return new Dir(handle, path, options);
}

module.exports = {
  Dir,
  opendir,
  opendirSync
};
