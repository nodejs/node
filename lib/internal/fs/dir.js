'use strict';

const {
  ArrayPrototypePush,
  ArrayPrototypeShift,
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
    ERR_MISSING_ARGS,
  },
} = require('internal/errors');

const { FSReqCallback } = binding;
const internalUtil = require('internal/util');
const {
  getDirent,
  getOptions,
  getValidatedPath,
  handleErrorFromBinding,
} = require('internal/fs/utils');
const {
  validateFunction,
  validateUint32,
} = require('internal/validators');

class Dir {
  #dirHandle;
  #dirPath;
  #dirBufferedEntries;
  #dirClosed;
  #dirOptions;
  #dirReadPromisified;
  #dirClosePromisified;
  #dirOperationQueue;

  constructor(handle, path, options) {
    if (handle == null) throw new ERR_MISSING_ARGS('handle');
    this.#dirHandle = handle;
    this.#dirBufferedEntries = [];
    this.#dirPath = path;
    this.#dirClosed = false;

    // Either `null` or an Array of pending operations (= functions to be called
    // once the current operation is done).
    this.#dirOperationQueue = null;

    this.#dirOptions = {
      bufferSize: 32,
      ...getOptions(options, {
        encoding: 'utf8',
      }),
    };

    validateUint32(this.#dirOptions.bufferSize, 'options.bufferSize', true);

    this.#dirReadPromisified = FunctionPrototypeBind(
      internalUtil.promisify(this.#dirReadImpl), this, false);
    this.#dirClosePromisified = FunctionPrototypeBind(
      internalUtil.promisify(this.close), this);
  }

  get path() {
    return this.#dirPath;
  }

  read(callback) {
    return this.#dirReadImpl(true, callback);
  }

  #dirReadImpl(maybeSync, callback) {
    if (this.#dirClosed === true) {
      throw new ERR_DIR_CLOSED();
    }

    if (callback === undefined) {
      return this.#dirReadPromisified();
    }

    validateFunction(callback, 'callback');

    if (this.#dirOperationQueue !== null) {
      ArrayPrototypePush(this.#dirOperationQueue, () => {
        this.#dirReadImpl(maybeSync, callback);
      });
      return;
    }

    if (this.#dirBufferedEntries.length > 0) {
      try {
        const dirent = ArrayPrototypeShift(this.#dirBufferedEntries);

        if (this.#dirOptions.recursive && dirent.isDirectory()) {
          this.readSyncRecursive(dirent);
        }

        if (maybeSync)
          process.nextTick(callback, null, dirent);
        else
          callback(null, dirent);
        return;
      } catch (error) {
        return callback(error);
      }
    }

    const req = new FSReqCallback();
    req.oncomplete = (err, result) => {
      process.nextTick(() => {
        const queue = this.#dirOperationQueue;
        this.#dirOperationQueue = null;
        for (const op of queue) op();
      });

      if (err || result === null) {
        return callback(err, result);
      }

      try {
        this.processReadResult(this.#dirPath, result);
        const dirent = ArrayPrototypeShift(this.#dirBufferedEntries);
        if (this.#dirOptions.recursive && dirent.isDirectory()) {
          this.readSyncRecursive(dirent);
        }
        callback(null, dirent);
      } catch (error) {
        callback(error);
      }
    };

    this.#dirOperationQueue = [];
    this.#dirHandle.read(
      this.#dirOptions.encoding,
      this.#dirOptions.bufferSize,
      req,
    );
  }

  processReadResult(path, result) {
    for (let i = 0; i < result.length; i += 2) {
      ArrayPrototypePush(
        this.#dirBufferedEntries,
        getDirent(
          path,
          result[i],
          result[i + 1],
        ),
      );
    }
  }

  readSyncRecursive(dirent) {
    const path = pathModule.join(dirent.parentPath, dirent.name);
    const ctx = { path };
    const handle = dirBinding.opendir(
      pathModule.toNamespacedPath(path),
      this.#dirOptions.encoding,
      undefined,
      ctx,
    );
    handleErrorFromBinding(ctx);
    const result = handle.read(
      this.#dirOptions.encoding,
      this.#dirOptions.bufferSize,
      undefined,
      ctx,
    );

    if (result) {
      this.processReadResult(path, result);
    }

    handle.close(undefined, ctx);
    handleErrorFromBinding(ctx);
  }

  readSync() {
    if (this.#dirClosed === true) {
      throw new ERR_DIR_CLOSED();
    }

    if (this.#dirOperationQueue !== null) {
      throw new ERR_DIR_CONCURRENT_OPERATION();
    }

    if (this.#dirBufferedEntries.length > 0) {
      const dirent = ArrayPrototypeShift(this.#dirBufferedEntries);
      if (this.#dirOptions.recursive && dirent.isDirectory()) {
        this.readSyncRecursive(dirent);
      }
      return dirent;
    }

    const ctx = { path: this.#dirPath };
    const result = this.#dirHandle.read(
      this.#dirOptions.encoding,
      this.#dirOptions.bufferSize,
      undefined,
      ctx,
    );
    handleErrorFromBinding(ctx);

    if (result === null) {
      return result;
    }

    this.processReadResult(this.#dirPath, result);

    const dirent = ArrayPrototypeShift(this.#dirBufferedEntries);
    if (this.#dirOptions.recursive && dirent.isDirectory()) {
      this.readSyncRecursive(dirent);
    }
    return dirent;
  }

  close(callback) {
    // Promise
    if (callback === undefined) {
      if (this.#dirClosed === true) {
        return PromiseReject(new ERR_DIR_CLOSED());
      }
      return this.#dirClosePromisified();
    }

    // callback
    validateFunction(callback, 'callback');

    if (this.#dirClosed === true) {
      process.nextTick(callback, new ERR_DIR_CLOSED());
      return;
    }

    if (this.#dirOperationQueue !== null) {
      ArrayPrototypePush(this.#dirOperationQueue, () => {
        this.close(callback);
      });
      return;
    }

    this.#dirClosed = true;
    const req = new FSReqCallback();
    req.oncomplete = callback;
    this.#dirHandle.close(req);
  }

  closeSync() {
    if (this.#dirClosed === true) {
      throw new ERR_DIR_CLOSED();
    }

    if (this.#dirOperationQueue !== null) {
      throw new ERR_DIR_CONCURRENT_OPERATION();
    }

    this.#dirClosed = true;
    const ctx = { path: this.#dirPath };
    const result = this.#dirHandle.close(undefined, ctx);
    handleErrorFromBinding(ctx);
    return result;
  }

  async* entries() {
    try {
      while (true) {
        const result = await this.#dirReadPromisified();
        if (result === null) {
          break;
        }
        yield result;
      }
    } finally {
      await this.#dirClosePromisified();
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
    encoding: 'utf8',
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
    req,
  );
}

function opendirSync(path, options) {
  path = getValidatedPath(path);
  options = getOptions(options, { encoding: 'utf8' });

  const handle = dirBinding.opendirSync(
    pathModule.toNamespacedPath(path),
  );

  return new Dir(handle, path, options);
}

module.exports = {
  Dir,
  opendir,
  opendirSync,
};
