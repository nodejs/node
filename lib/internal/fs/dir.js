'use strict';

const {
  ArrayPrototypePush,
  ArrayPrototypeSlice,
  ArrayPrototypeSplice,
  FunctionPrototypeBind,
  ObjectDefineProperty,
  PromiseReject,
  SymbolAsyncIterator,
} = primordials;

const pathModule = require('path');
const binding = internalBinding('fs');
const dirBinding = internalBinding('fs_dir');
const {
  codes: {
    ERR_DIR_CLOSED,
    ERR_DIR_CONCURRENT_OPERATION,
    ERR_INVALID_CALLBACK,
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
  validateUint32
} = require('internal/validators');

class Dir {
  #handle = undefined;
  #path = undefined;
  #options = undefined;
  #bufferedEntries = [];
  #dirClosed = false;
  // Either `null` or an Array of pending operations (= functions to be called
  // once the current operation is done).
  #operationQueue = null;
  #readPromisified = undefined;
  #closePromisified = undefined;

  constructor(handle, path, options) {
    if (handle == null) throw new ERR_MISSING_ARGS('handle');
    this.#handle = handle;
    this.#path = path;
    this.#options = {
      bufferSize: 32,
      ...getOptions(options, {
        encoding: 'utf8'
      })
    };

    this.#readPromisified = FunctionPrototypeBind(
      internalUtil.promisify(this.#readImpl), this, false);
    this.#closePromisified = this.#closePromisified = FunctionPrototypeBind(
      internalUtil.promisify(this.close), this);

    validateUint32(this.#options.bufferSize, 'options.bufferSize', true);
  }

  get path() {
    return this.#path;
  }

  read(callback) {
    return this.#readImpl(true, callback);
  }

  #readImpl(maybeSync, callback) {
    if (this.#dirClosed === true) {
      throw new ERR_DIR_CLOSED();
    }

    if (callback === undefined) {
      return this.#readPromisified();
    } else if (typeof callback !== 'function') {
      throw new ERR_INVALID_CALLBACK(callback);
    }

    if (this.#operationQueue !== null) {
      ArrayPrototypePush(this.#operationQueue, () => {
        this.#readImpl(maybeSync, callback);
      });
      return;
    }

    if (this.#bufferedEntries.length > 0) {
      const [ name, type ] =
        ArrayPrototypeSplice(this.#bufferedEntries, 0, 2);
      if (maybeSync)
        process.nextTick(getDirent, this.#path, name, type, callback);
      else
        getDirent(this.#path, name, type, callback);
      return;
    }

    const req = new FSReqCallback();
    req.oncomplete = (err, result) => {
      process.nextTick(() => {
        const queue = this.#operationQueue;
        this.#operationQueue = null;
        for (const op of queue) op();
      });

      if (err || result === null) {
        return callback(err, result);
      }

      this.#bufferedEntries = ArrayPrototypeSlice(result, 2);
      getDirent(this.#path, result[0], result[1], callback);
    };

    this.#operationQueue = [];
    this.#handle.read(
      this.#options.encoding,
      this.#options.bufferSize,
      req
    );
  }

  readSync() {
    if (this.#dirClosed === true) {
      throw new ERR_DIR_CLOSED();
    }

    if (this.#operationQueue !== null) {
      throw new ERR_DIR_CONCURRENT_OPERATION();
    }

    if (this.#bufferedEntries.length > 0) {
      const [ name, type ] =
          ArrayPrototypeSplice(this.#bufferedEntries, 0, 2);
      return getDirent(this.#path, name, type);
    }

    const ctx = { path: this.#path };
    const result = this.#handle.read(
      this.#options.encoding,
      this.#options.bufferSize,
      undefined,
      ctx
    );
    handleErrorFromBinding(ctx);

    if (result === null) {
      return result;
    }

    this.#bufferedEntries = ArrayPrototypeSlice(result, 2);
    return getDirent(this.#path, result[0], result[1]);
  }

  close(callback) {
    // Promise
    if (callback === undefined) {
      if (this.#dirClosed === true) {
        return PromiseReject(new ERR_DIR_CLOSED());
      }
      return this.#closePromisified();
    }

    // callback
    if (typeof callback !== 'function') {
      throw new ERR_INVALID_CALLBACK(callback);
    }

    if (this.#dirClosed === true) {
      process.nextTick(callback, new ERR_DIR_CLOSED());
      return;
    }

    if (this.#operationQueue !== null) {
      this.#operationQueue.push(() => {
        this.close(callback);
      });
      return;
    }

    this.#dirClosed = true;
    const req = new FSReqCallback();
    req.oncomplete = callback;
    this.#handle.close(req);
  }

  closeSync() {
    if (this.#dirClosed === true) {
      throw new ERR_DIR_CLOSED();
    }

    if (this.#operationQueue !== null) {
      throw new ERR_DIR_CONCURRENT_OPERATION();
    }

    this.#dirClosed = true;
    const ctx = { path: this.#path };
    const result = this.#handle.close(undefined, ctx);
    handleErrorFromBinding(ctx);
    return result;
  }

  async* entries() {
    try {
      while (true) {
        const result = await this.#readPromisified();
        if (result === null) {
          break;
        }
        yield result;
      }
    } finally {
      await this.#closePromisified();
    }
  }
}

ObjectDefineProperty(Dir.prototype, SymbolAsyncIterator, {
  value: Dir.prototype.entries,
  enumerable: false,
  writable: true,
  configurable: true,
});

function opendir(path, options, callback) {
  callback = typeof options === 'function' ? options : callback;
  if (typeof callback !== 'function') {
    throw new ERR_INVALID_CALLBACK(callback);
  }
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
