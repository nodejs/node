'use strict';

const {
  ObjectDefineProperty,
  Symbol,
  SymbolAsyncIterator,
} = primordials;

const pathModule = require('path');
const binding = internalBinding('fs');
const dirBinding = internalBinding('fs_dir');
const {
  codes: {
    ERR_DIR_CLOSED,
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

const kDirHandle = Symbol('kDirHandle');
const kDirPath = Symbol('kDirPath');
const kDirBufferedEntries = Symbol('kDirBufferedEntries');
const kDirClosed = Symbol('kDirClosed');
const kDirOptions = Symbol('kDirOptions');
const kDirReadImpl = Symbol('kDirReadImpl');
const kDirReadPromisified = Symbol('kDirReadPromisified');
const kDirClosePromisified = Symbol('kDirClosePromisified');

class Dir {
  constructor(handle, path, options) {
    if (handle == null) throw new ERR_MISSING_ARGS('handle');
    this[kDirHandle] = handle;
    this[kDirBufferedEntries] = [];
    this[kDirPath] = path;
    this[kDirClosed] = false;

    this[kDirOptions] = {
      bufferSize: 32,
      ...getOptions(options, {
        encoding: 'utf8'
      })
    };

    validateUint32(this[kDirOptions].bufferSize, 'options.bufferSize', true);

    this[kDirReadPromisified] =
        internalUtil.promisify(this[kDirReadImpl]).bind(this, false);
    this[kDirClosePromisified] = internalUtil.promisify(this.close).bind(this);
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
    } else if (typeof callback !== 'function') {
      throw new ERR_INVALID_CALLBACK(callback);
    }

    if (this[kDirBufferedEntries].length > 0) {
      const [ name, type ] = this[kDirBufferedEntries].splice(0, 2);
      if (maybeSync)
        process.nextTick(getDirent, this[kDirPath], name, type, callback);
      else
        getDirent(this[kDirPath], name, type, callback);
      return;
    }

    const req = new FSReqCallback();
    req.oncomplete = (err, result) => {
      if (err || result === null) {
        return callback(err, result);
      }

      this[kDirBufferedEntries] = result.slice(2);
      getDirent(this[kDirPath], result[0], result[1], callback);
    };

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

    if (this[kDirBufferedEntries].length > 0) {
      const [ name, type ] = this[kDirBufferedEntries].splice(0, 2);
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

    this[kDirBufferedEntries] = result.slice(2);
    return getDirent(this[kDirPath], result[0], result[1]);
  }

  close(callback) {
    if (this[kDirClosed] === true) {
      throw new ERR_DIR_CLOSED();
    }

    if (callback === undefined) {
      return this[kDirClosePromisified]();
    } else if (typeof callback !== 'function') {
      throw new ERR_INVALID_CALLBACK(callback);
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
