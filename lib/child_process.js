// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';

const {
  ArrayIsArray,
  ArrayPrototypeFilter,
  ArrayPrototypeIncludes,
  ArrayPrototypeJoin,
  ArrayPrototypeLastIndexOf,
  ArrayPrototypePush,
  ArrayPrototypeSlice,
  ArrayPrototypeSort,
  ArrayPrototypeSplice,
  ArrayPrototypeUnshift,
  Error,
  NumberIsInteger,
  ObjectAssign,
  ObjectDefineProperty,
  ObjectPrototypeHasOwnProperty,
  RegExpPrototypeTest,
  SafeSet,
  StringPrototypeSlice,
  StringPrototypeToUpperCase,
} = primordials;

const {
  promisify,
  convertToValidSignal,
  createDeferredPromise,
  getSystemErrorName
} = require('internal/util');
const { isArrayBufferView } = require('internal/util/types');
let debug = require('internal/util/debuglog').debuglog(
  'child_process',
  (fn) => {
    debug = fn;
  }
);
const { Buffer } = require('buffer');
const { Pipe, constants: PipeConstants } = internalBinding('pipe_wrap');

const {
  AbortError,
  codes: errorCodes,
} = require('internal/errors');
const {
  ERR_INVALID_ARG_VALUE,
  ERR_CHILD_PROCESS_IPC_REQUIRED,
  ERR_CHILD_PROCESS_STDIO_MAXBUFFER,
  ERR_INVALID_ARG_TYPE,
  ERR_OUT_OF_RANGE,
} = errorCodes;
const { clearTimeout, setTimeout } = require('timers');
const { getValidatedPath } = require('internal/fs/utils');
const {
  isInt32,
  validateAbortSignal,
  validateBoolean,
  validateObject,
  validateString,
} = require('internal/validators');
const child_process = require('internal/child_process');
const {
  getValidStdio,
  setupChannel,
  ChildProcess,
  stdioStringToArray
} = child_process;

const MAX_BUFFER = 1024 * 1024;

/**
 * Spawns a new Node.js process + fork.
 * @param {string} modulePath
 * @param {string[]} [args]
 * @param {{
 *   cwd?: string;
 *   detached?: boolean;
 *   env?: Object;
 *   execPath?: string;
 *   execArgv?: string[];
 *   gid?: number;
 *   serialization?: string;
 *   signal?: AbortSignal;
 *   killSignal?: string | number;
 *   silent?: boolean;
 *   stdio?: Array | string;
 *   uid?: number;
 *   windowsVerbatimArguments?: boolean;
 *   timeout?: number;
 *   }} [options]
 * @returns {ChildProcess}
 */
function fork(modulePath /* , args, options */) {
  validateString(modulePath, 'modulePath');

  // Get options and args arguments.
  let execArgv;
  let options = {};
  let args = [];
  let pos = 1;
  if (pos < arguments.length && ArrayIsArray(arguments[pos])) {
    args = arguments[pos++];
  }

  if (pos < arguments.length && arguments[pos] == null) {
    pos++;
  }

  if (pos < arguments.length && arguments[pos] != null) {
    if (typeof arguments[pos] !== 'object') {
      throw new ERR_INVALID_ARG_VALUE(`arguments[${pos}]`, arguments[pos]);
    }

    options = { ...arguments[pos++] };
  }

  // Prepare arguments for fork:
  execArgv = options.execArgv || process.execArgv;

  if (execArgv === process.execArgv && process._eval != null) {
    const index = ArrayPrototypeLastIndexOf(execArgv, process._eval);
    if (index > 0) {
      // Remove the -e switch to avoid fork bombing ourselves.
      execArgv = ArrayPrototypeSlice(execArgv);
      ArrayPrototypeSplice(execArgv, index - 1, 2);
    }
  }

  args = [...execArgv, modulePath, ...args];

  if (typeof options.stdio === 'string') {
    options.stdio = stdioStringToArray(options.stdio, 'ipc');
  } else if (!ArrayIsArray(options.stdio)) {
    // Use a separate fd=3 for the IPC channel. Inherit stdin, stdout,
    // and stderr from the parent if silent isn't set.
    options.stdio = stdioStringToArray(
      options.silent ? 'pipe' : 'inherit',
      'ipc');
  } else if (!ArrayPrototypeIncludes(options.stdio, 'ipc')) {
    throw new ERR_CHILD_PROCESS_IPC_REQUIRED('options.stdio');
  }

  options.execPath = options.execPath || process.execPath;
  options.shell = false;

  return spawn(options.execPath, args, options);
}

function _forkChild(fd, serializationMode) {
  // set process.send()
  const p = new Pipe(PipeConstants.IPC);
  p.open(fd);
  p.unref();
  const control = setupChannel(process, p, serializationMode);
  process.on('newListener', function onNewListener(name) {
    if (name === 'message' || name === 'disconnect') control.refCounted();
  });
  process.on('removeListener', function onRemoveListener(name) {
    if (name === 'message' || name === 'disconnect') control.unrefCounted();
  });
}

function normalizeExecArgs(command, options, callback) {
  if (typeof options === 'function') {
    callback = options;
    options = undefined;
  }

  // Make a shallow copy so we don't clobber the user's options object.
  options = { ...options };
  options.shell = typeof options.shell === 'string' ? options.shell : true;

  return {
    file: command,
    options: options,
    callback: callback
  };
}

/**
 * Spawns a shell executing the given command.
 * @param {string} command
 * @param {{
 *   cmd?: string;
 *   env?: Object;
 *   encoding?: string;
 *   shell?: string;
 *   signal?: AbortSignal;
 *   timeout?: number;
 *   maxBuffer?: number;
 *   killSignal?: string | number;
 *   uid?: number;
 *   gid?: number;
 *   windowsHide?: boolean;
 *   }} [options]
 * @param {(
 *   error?: Error,
 *   stdout?: string | Buffer,
 *   stderr?: string | Buffer
 *   ) => any} [callback]
 * @returns {ChildProcess}
 */
function exec(command, options, callback) {
  const opts = normalizeExecArgs(command, options, callback);
  return module.exports.execFile(opts.file,
                                 opts.options,
                                 opts.callback);
}

const customPromiseExecFunction = (orig) => {
  return (...args) => {
    const { promise, resolve, reject } = createDeferredPromise();

    promise.child = orig(...args, (err, stdout, stderr) => {
      if (err !== null) {
        err.stdout = stdout;
        err.stderr = stderr;
        reject(err);
      } else {
        resolve({ stdout, stderr });
      }
    });

    return promise;
  };
};

ObjectDefineProperty(exec, promisify.custom, {
  enumerable: false,
  value: customPromiseExecFunction(exec)
});

/**
 * Spawns the specified file as a shell.
 * @param {string} file
 * @param {string[]} [args]
 * @param {{
 *   cwd?: string;
 *   env?: Object;
 *   encoding?: string;
 *   timeout?: number;
 *   maxBuffer?: number;
 *   killSignal?: string | number;
 *   uid?: number;
 *   gid?: number;
 *   windowsHide?: boolean;
 *   windowsVerbatimArguments?: boolean;
 *   shell?: boolean | string;
 *   signal?: AbortSignal;
 *   }} [options]
 * @param {(
 *   error?: Error,
 *   stdout?: string | Buffer,
 *   stderr?: string | Buffer
 *   ) => any} [callback]
 * @returns {ChildProcess}
 */
function execFile(file /* , args, options, callback */) {
  let args = [];
  let callback;
  let options;

  // Parse the optional positional parameters.
  let pos = 1;
  if (pos < arguments.length && ArrayIsArray(arguments[pos])) {
    args = arguments[pos++];
  } else if (pos < arguments.length && arguments[pos] == null) {
    pos++;
  }

  if (pos < arguments.length && typeof arguments[pos] === 'object') {
    options = arguments[pos++];
  } else if (pos < arguments.length && arguments[pos] == null) {
    pos++;
  }

  if (pos < arguments.length && typeof arguments[pos] === 'function') {
    callback = arguments[pos++];
  }

  if (!callback && pos < arguments.length && arguments[pos] != null) {
    throw new ERR_INVALID_ARG_VALUE('args', arguments[pos]);
  }

  options = {
    encoding: 'utf8',
    timeout: 0,
    maxBuffer: MAX_BUFFER,
    killSignal: 'SIGTERM',
    cwd: null,
    env: null,
    shell: false,
    ...options
  };

  // Validate the timeout, if present.
  validateTimeout(options.timeout);

  // Validate maxBuffer, if present.
  validateMaxBuffer(options.maxBuffer);

  options.killSignal = sanitizeKillSignal(options.killSignal);

  const child = spawn(file, args, {
    cwd: options.cwd,
    env: options.env,
    gid: options.gid,
    shell: options.shell,
    signal: options.signal,
    uid: options.uid,
    windowsHide: !!options.windowsHide,
    windowsVerbatimArguments: !!options.windowsVerbatimArguments
  });

  let encoding;
  const _stdout = [];
  const _stderr = [];
  if (options.encoding !== 'buffer' && Buffer.isEncoding(options.encoding)) {
    encoding = options.encoding;
  } else {
    encoding = null;
  }
  let stdoutLen = 0;
  let stderrLen = 0;
  let killed = false;
  let exited = false;
  let timeoutId;

  let ex = null;

  let cmd = file;

  function exithandler(code, signal) {
    if (exited) return;
    exited = true;

    if (timeoutId) {
      clearTimeout(timeoutId);
      timeoutId = null;
    }

    if (!callback) return;

    // merge chunks
    let stdout;
    let stderr;
    if (encoding ||
      (
        child.stdout &&
        child.stdout.readableEncoding
      )) {
      stdout = ArrayPrototypeJoin(_stdout, '');
    } else {
      stdout = Buffer.concat(_stdout);
    }
    if (encoding ||
      (
        child.stderr &&
        child.stderr.readableEncoding
      )) {
      stderr = ArrayPrototypeJoin(_stderr, '');
    } else {
      stderr = Buffer.concat(_stderr);
    }

    if (!ex && code === 0 && signal === null) {
      callback(null, stdout, stderr);
      return;
    }

    if (args.length !== 0)
      cmd += ` ${ArrayPrototypeJoin(args, ' ')}`;

    if (!ex) {
      // eslint-disable-next-line no-restricted-syntax
      ex = new Error('Command failed: ' + cmd + '\n' + stderr);
      ex.killed = child.killed || killed;
      ex.code = code < 0 ? getSystemErrorName(code) : code;
      ex.signal = signal;
    }

    ex.cmd = cmd;
    callback(ex, stdout, stderr);
  }

  function errorhandler(e) {
    ex = e;

    if (child.stdout)
      child.stdout.destroy();

    if (child.stderr)
      child.stderr.destroy();

    exithandler();
  }

  function kill() {
    if (child.stdout)
      child.stdout.destroy();

    if (child.stderr)
      child.stderr.destroy();

    killed = true;
    try {
      child.kill(options.killSignal);
    } catch (e) {
      ex = e;
      exithandler();
    }
  }

  if (options.timeout > 0) {
    timeoutId = setTimeout(function delayedKill() {
      kill();
      timeoutId = null;
    }, options.timeout);
  }

  if (child.stdout) {
    if (encoding)
      child.stdout.setEncoding(encoding);

    child.stdout.on('data', function onChildStdout(chunk) {
      const encoding = child.stdout.readableEncoding;
      const length = encoding ?
        Buffer.byteLength(chunk, encoding) :
        chunk.length;
      const slice = encoding ? StringPrototypeSlice :
        (buf, ...args) => buf.slice(...args);
      stdoutLen += length;

      if (stdoutLen > options.maxBuffer) {
        const truncatedLen = options.maxBuffer - (stdoutLen - length);
        ArrayPrototypePush(_stdout, slice(chunk, 0, truncatedLen));

        ex = new ERR_CHILD_PROCESS_STDIO_MAXBUFFER('stdout');
        kill();
      } else {
        ArrayPrototypePush(_stdout, chunk);
      }
    });
  }

  if (child.stderr) {
    if (encoding)
      child.stderr.setEncoding(encoding);

    child.stderr.on('data', function onChildStderr(chunk) {
      const encoding = child.stderr.readableEncoding;
      const length = encoding ?
        Buffer.byteLength(chunk, encoding) :
        chunk.length;
      stderrLen += length;

      if (stderrLen > options.maxBuffer) {
        const truncatedLen = options.maxBuffer - (stderrLen - length);
        ArrayPrototypePush(_stderr,
                           chunk.slice(0, truncatedLen));

        ex = new ERR_CHILD_PROCESS_STDIO_MAXBUFFER('stderr');
        kill();
      } else {
        _stderr.push(chunk);
      }
    });
  }

  child.addListener('close', exithandler);
  child.addListener('error', errorhandler);

  return child;
}

ObjectDefineProperty(execFile, promisify.custom, {
  enumerable: false,
  value: customPromiseExecFunction(execFile)
});

function normalizeSpawnArguments(file, args, options) {
  validateString(file, 'file');

  if (file.length === 0)
    throw new ERR_INVALID_ARG_VALUE('file', file, 'cannot be empty');

  if (ArrayIsArray(args)) {
    args = ArrayPrototypeSlice(args);
  } else if (args == null) {
    args = [];
  } else if (typeof args !== 'object') {
    throw new ERR_INVALID_ARG_TYPE('args', 'object', args);
  } else {
    options = args;
    args = [];
  }

  if (options === undefined)
    options = {};
  else
    validateObject(options, 'options');

  let cwd = options.cwd;

  // Validate the cwd, if present.
  if (cwd != null) {
    cwd = getValidatedPath(cwd, 'options.cwd');
  }

  // Validate detached, if present.
  if (options.detached != null) {
    validateBoolean(options.detached, 'options.detached');
  }

  // Validate the uid, if present.
  if (options.uid != null && !isInt32(options.uid)) {
    throw new ERR_INVALID_ARG_TYPE('options.uid', 'int32', options.uid);
  }

  // Validate the gid, if present.
  if (options.gid != null && !isInt32(options.gid)) {
    throw new ERR_INVALID_ARG_TYPE('options.gid', 'int32', options.gid);
  }

  // Validate the shell, if present.
  if (options.shell != null &&
      typeof options.shell !== 'boolean' &&
      typeof options.shell !== 'string') {
    throw new ERR_INVALID_ARG_TYPE('options.shell',
                                   ['boolean', 'string'], options.shell);
  }

  // Validate argv0, if present.
  if (options.argv0 != null) {
    validateString(options.argv0, 'options.argv0');
  }

  // Validate windowsHide, if present.
  if (options.windowsHide != null) {
    validateBoolean(options.windowsHide, 'options.windowsHide');
  }

  // Validate windowsVerbatimArguments, if present.
  let { windowsVerbatimArguments } = options;
  if (windowsVerbatimArguments != null) {
    validateBoolean(windowsVerbatimArguments,
                    'options.windowsVerbatimArguments');
  }

  if (options.shell) {
    const command = ArrayPrototypeJoin([file, ...args], ' ');
    // Set the shell, switches, and commands.
    if (process.platform === 'win32') {
      if (typeof options.shell === 'string')
        file = options.shell;
      else
        file = process.env.comspec || 'cmd.exe';
      // '/d /s /c' is used only for cmd.exe.
      if (RegExpPrototypeTest(/^(?:.*\\)?cmd(?:\.exe)?$/i, file)) {
        args = ['/d', '/s', '/c', `"${command}"`];
        windowsVerbatimArguments = true;
      } else {
        args = ['-c', command];
      }
    } else {
      if (typeof options.shell === 'string')
        file = options.shell;
      else if (process.platform === 'android')
        file = '/system/bin/sh';
      else
        file = '/bin/sh';
      args = ['-c', command];
    }
  }

  if (typeof options.argv0 === 'string') {
    ArrayPrototypeUnshift(args, options.argv0);
  } else {
    ArrayPrototypeUnshift(args, file);
  }

  const env = options.env || process.env;
  const envPairs = [];

  // process.env.NODE_V8_COVERAGE always propagates, making it possible to
  // collect coverage for programs that spawn with white-listed environment.
  if (process.env.NODE_V8_COVERAGE &&
      !ObjectPrototypeHasOwnProperty(options.env || {}, 'NODE_V8_COVERAGE')) {
    env.NODE_V8_COVERAGE = process.env.NODE_V8_COVERAGE;
  }

  let envKeys = [];
  // Prototype values are intentionally included.
  for (const key in env) {
    ArrayPrototypePush(envKeys, key);
  }

  if (process.platform === 'win32') {
    // On Windows env keys are case insensitive. Filter out duplicates,
    // keeping only the first one (in lexicographic order)
    const sawKey = new SafeSet();
    envKeys = ArrayPrototypeFilter(
      ArrayPrototypeSort(envKeys),
      (key) => {
        const uppercaseKey = StringPrototypeToUpperCase(key);
        if (sawKey.has(uppercaseKey)) {
          return false;
        }
        sawKey.add(uppercaseKey);
        return true;
      }
    );
  }

  for (const key of envKeys) {
    const value = env[key];
    if (value !== undefined) {
      ArrayPrototypePush(envPairs, `${key}=${value}`);
    }
  }

  return {
    // Make a shallow copy so we don't clobber the user's options object.
    ...options,
    args,
    cwd,
    detached: !!options.detached,
    envPairs,
    file,
    windowsHide: !!options.windowsHide,
    windowsVerbatimArguments: !!windowsVerbatimArguments,
  };
}

function abortChildProcess(child, killSignal) {
  if (!child)
    return;
  try {
    if (child.kill(killSignal)) {
      child.emit('error', new AbortError());
    }
  } catch (err) {
    child.emit('error', err);
  }
}

/**
 * Spawns a new process using the given `file`.
 * @param {string} file
 * @param {string[]} [args]
 * @param {{
 *   cwd?: string;
 *   env?: Object;
 *   argv0?: string;
 *   stdio?: Array | string;
 *   detached?: boolean;
 *   uid?: number;
 *   gid?: number;
 *   serialization?: string;
 *   shell?: boolean | string;
 *   windowsVerbatimArguments?: boolean;
 *   windowsHide?: boolean;
 *   signal?: AbortSignal;
 *   timeout?: number;
 *   killSignal?: string | number;
 *   }} [options]
 * @returns {ChildProcess}
 */
function spawn(file, args, options) {
  options = normalizeSpawnArguments(file, args, options);
  validateTimeout(options.timeout);
  validateAbortSignal(options.signal, 'options.signal');
  const killSignal = sanitizeKillSignal(options.killSignal);
  const child = new ChildProcess();

  debug('spawn', options);
  child.spawn(options);

  if (options.timeout > 0) {
    let timeoutId = setTimeout(() => {
      if (timeoutId) {
        try {
          child.kill(killSignal);
        } catch (err) {
          child.emit('error', err);
        }
        timeoutId = null;
      }
    }, options.timeout);

    child.once('exit', () => {
      if (timeoutId) {
        clearTimeout(timeoutId);
        timeoutId = null;
      }
    });
  }

  if (options.signal) {
    const signal = options.signal;
    if (signal.aborted) {
      process.nextTick(onAbortListener);
    } else {
      signal.addEventListener('abort', onAbortListener, { once: true });
      child.once('exit',
                 () => signal.removeEventListener('abort', onAbortListener));
    }

    function onAbortListener() {
      abortChildProcess(child, killSignal);
    }
  }

  return child;
}

/**
 * Spawns a new process synchronously using the given `file`.
 * @param {string} file
 * @param {string[]} [args]
 * @param {{
 *   cwd?: string;
 *   input?: string | Buffer | TypedArray | DataView;
 *   argv0?: string;
 *   stdio?: string | Array;
 *   env?: Object;
 *   uid?: number;
 *   gid?: number;
 *   timeout?: number;
 *   killSignal?: string | number;
 *   maxBuffer?: number;
 *   encoding?: string;
 *   shell?: boolean | string;
 *   windowsVerbatimArguments?: boolean;
 *   windowsHide?: boolean;
 *   }} [options]
 * @returns {{
 *   pid: number;
 *   output: Array;
 *   stdout: Buffer | string;
 *   stderr: Buffer | string;
 *   status: number | null;
 *   signal: string | null;
 *   error: Error;
 *   }}
 */
function spawnSync(file, args, options) {
  options = {
    maxBuffer: MAX_BUFFER,
    ...normalizeSpawnArguments(file, args, options)
  };

  debug('spawnSync', options);

  // Validate the timeout, if present.
  validateTimeout(options.timeout);

  // Validate maxBuffer, if present.
  validateMaxBuffer(options.maxBuffer);

  // Validate and translate the kill signal, if present.
  options.killSignal = sanitizeKillSignal(options.killSignal);

  options.stdio = getValidStdio(options.stdio || 'pipe', true).stdio;

  if (options.input) {
    const stdin = options.stdio[0] = { ...options.stdio[0] };
    stdin.input = options.input;
  }

  // We may want to pass data in on any given fd, ensure it is a valid buffer
  for (let i = 0; i < options.stdio.length; i++) {
    const input = options.stdio[i] && options.stdio[i].input;
    if (input != null) {
      const pipe = options.stdio[i] = { ...options.stdio[i] };
      if (isArrayBufferView(input)) {
        pipe.input = input;
      } else if (typeof input === 'string') {
        pipe.input = Buffer.from(input, options.encoding);
      } else {
        throw new ERR_INVALID_ARG_TYPE(`options.stdio[${i}]`,
                                       ['Buffer',
                                        'TypedArray',
                                        'DataView',
                                        'string'],
                                       input);
      }
    }
  }

  return child_process.spawnSync(options);
}


function checkExecSyncError(ret, args, cmd) {
  let err;
  if (ret.error) {
    err = ret.error;
  } else if (ret.status !== 0) {
    let msg = 'Command failed: ';
    msg += cmd || ArrayPrototypeJoin(args, ' ');
    if (ret.stderr && ret.stderr.length > 0)
      msg += `\n${ret.stderr.toString()}`;
    // eslint-disable-next-line no-restricted-syntax
    err = new Error(msg);
  }
  if (err) {
    ObjectAssign(err, ret);
  }
  return err;
}

/**
 * Spawns a file as a shell synchronously.
 * @param {string} command
 * @param {string[]} [args]
 * @param {{
 *   cwd?: string;
 *   input?: string | Buffer | TypedArray | DataView;
 *   stdio?: string | Array;
 *   env?: Object;
 *   uid?: number;
 *   gid?: number;
 *   timeout?: number;
 *   killSignal?: string | number;
 *   maxBuffer?: number;
 *   encoding?: string;
 *   windowsHide?: boolean;
 *   shell?: boolean | string;
 *   }} [options]
 * @returns {Buffer | string}
 */
function execFileSync(command, args, options) {
  options = normalizeSpawnArguments(command, args, options);

  const inheritStderr = !options.stdio;
  const ret = spawnSync(options.file,
                        ArrayPrototypeSlice(options.args, 1), options);

  if (inheritStderr && ret.stderr)
    process.stderr.write(ret.stderr);

  const err = checkExecSyncError(ret, options.args, undefined);

  if (err)
    throw err;

  return ret.stdout;
}

/**
 * Spawns a shell executing the given `command` synchronously.
 * @param {string} command
 * @param {{
 *   cwd?: string;
 *   input?: string | Buffer | TypedArray | DataView;
 *   stdio?: string | Array;
 *   env?: Object;
 *   shell?: string;
 *   uid?: number;
 *   gid?: number;
 *   timeout?: number;
 *   killSignal?: string | number;
 *   maxBuffer?: number;
 *   encoding?: string;
 *   windowsHide?: boolean;
 *   }} [options]
 * @returns {Buffer | string}
 */
function execSync(command, options) {
  const opts = normalizeExecArgs(command, options, null);
  const inheritStderr = !opts.options.stdio;

  const ret = spawnSync(opts.file, opts.options);

  if (inheritStderr && ret.stderr)
    process.stderr.write(ret.stderr);

  const err = checkExecSyncError(ret, opts.args, command);

  if (err)
    throw err;

  return ret.stdout;
}


function validateTimeout(timeout) {
  if (timeout != null && !(NumberIsInteger(timeout) && timeout >= 0)) {
    throw new ERR_OUT_OF_RANGE('timeout', 'an unsigned integer', timeout);
  }
}


function validateMaxBuffer(maxBuffer) {
  if (maxBuffer != null && !(typeof maxBuffer === 'number' && maxBuffer >= 0)) {
    throw new ERR_OUT_OF_RANGE('options.maxBuffer',
                               'a positive number',
                               maxBuffer);
  }
}


function sanitizeKillSignal(killSignal) {
  if (typeof killSignal === 'string' || typeof killSignal === 'number') {
    return convertToValidSignal(killSignal);
  } else if (killSignal != null) {
    throw new ERR_INVALID_ARG_TYPE('options.killSignal',
                                   ['string', 'number'],
                                   killSignal);
  }
}

module.exports = {
  _forkChild,
  ChildProcess,
  exec,
  execFile,
  execFileSync,
  execSync,
  fork,
  spawn,
  spawnSync
};
