'use strict';

const { Object } = primordials;

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

const kDirHandle = Symbol('kDirHandle');
const kDirPath = Symbol('kDirPath');
const kDirClosed = Symbol('kDirClosed');
const kDirOptions = Symbol('kDirOptions');
const kDirReadPromisified = Symbol('kDirReadPromisified');
const kDirClosePromisified = Symbol('kDirClosePromisified');

class Dir {
  constructor(handle, path, options) {
    if (handle == null) throw new ERR_MISSING_ARGS('handle');
    this[kDirHandle] = handle;
    this[kDirPath] = path;
    this[kDirClosed] = false;

    this[kDirOptions] = getOptions(options, {
      encoding: 'utf8'
    });

    this[kDirReadPromisified] = internalUtil.promisify(this.read).bind(this);
    this[kDirClosePromisified] = internalUtil.promisify(this.close).bind(this);
  }

  get path() {
    return this[kDirPath];
  }

  read(options, callback) {
    if (this[kDirClosed] === true) {
      throw new ERR_DIR_CLOSED();
    }

    callback = typeof options === 'function' ? options : callback;
    if (callback === undefined) {
      return this[kDirReadPromisified](options);
    } else if (typeof callback !== 'function') {
      throw new ERR_INVALID_CALLBACK(callback);
    }
    options = getOptions(options, this[kDirOptions]);

    const req = new FSReqCallback();
    req.oncomplete = (err, result) => {
      if (err || result === null) {
        return callback(err, result);
      }
      getDirent(this[kDirPath], result[0], result[1], callback);
    };

    this[kDirHandle].read(
      options.encoding,
      req
    );
  }

  readSync(options) {
    if (this[kDirClosed] === true) {
      throw new ERR_DIR_CLOSED();
    }

    options = getOptions(options, this[kDirOptions]);

    const ctx = { path: this[kDirPath] };
    const result = this[kDirHandle].read(
      options.encoding,
      undefined,
      ctx
    );
    handleErrorFromBinding(ctx);

    if (result === null) {
      return result;
    }

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

Object.defineProperty(Dir.prototype, Symbol.asyncIterator, {
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
