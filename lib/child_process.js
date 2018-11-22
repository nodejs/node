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

const util = require('util');
const { getSystemErrorName } = require('internal/util');
const { isArrayBufferView } = require('internal/util/types');
const { Buffer } = require('buffer');
const { Pipe, constants: PipeConstants } = internalBinding('pipe_wrap');
const {
  ERR_INVALID_ARG_VALUE,
  ERR_CHILD_PROCESS_IPC_REQUIRED,
  ERR_CHILD_PROCESS_STDIO_MAXBUFFER,
  ERR_INVALID_ARG_TYPE,
  ERR_INVALID_OPT_VALUE
} = require('internal/errors').codes;
const child_process = require('internal/child_process/base');
const {
  _validateStdio,
  setupChannel,
  ChildProcess
} = child_process;
const {
  debug,
  normalizeExecArgs,
  normalizeSpawnArguments,
  sanitizeKillSignal,
  spawn,
  validateMaxBuffer,
  validateTimeout
} = require('internal/child_process/utils');

// Lazy loaded
let promises;

let promisesWarn = true;

exports.ChildProcess = ChildProcess;

function stdioStringToArray(option) {
  switch (option) {
    case 'ignore':
    case 'pipe':
    case 'inherit':
      return [option, option, option, 'ipc'];
    default:
      throw new ERR_INVALID_OPT_VALUE('stdio', option);
  }
}

exports.fork = function fork(modulePath /* , args, options */) {

  // Get options and args arguments.
  var execArgv;
  var options = {};
  var args = [];
  var pos = 1;
  if (pos < arguments.length && Array.isArray(arguments[pos])) {
    args = arguments[pos++];
  }

  if (pos < arguments.length &&
      (arguments[pos] === undefined || arguments[pos] === null)) {
    pos++;
  }

  if (pos < arguments.length && arguments[pos] != null) {
    if (typeof arguments[pos] !== 'object') {
      throw new ERR_INVALID_ARG_VALUE(`arguments[${pos}]`, arguments[pos]);
    }

    options = util._extend({}, arguments[pos++]);
  }

  // Prepare arguments for fork:
  execArgv = options.execArgv || process.execArgv;

  if (execArgv === process.execArgv && process._eval != null) {
    const index = execArgv.lastIndexOf(process._eval);
    if (index > 0) {
      // Remove the -e switch to avoid fork bombing ourselves.
      execArgv = execArgv.slice();
      execArgv.splice(index - 1, 2);
    }
  }

  args = execArgv.concat([modulePath], args);

  if (typeof options.stdio === 'string') {
    options.stdio = stdioStringToArray(options.stdio);
  } else if (!Array.isArray(options.stdio)) {
    // Use a separate fd=3 for the IPC channel. Inherit stdin, stdout,
    // and stderr from the parent if silent isn't set.
    options.stdio = options.silent ? stdioStringToArray('pipe') :
      stdioStringToArray('inherit');
  } else if (options.stdio.indexOf('ipc') === -1) {
    throw new ERR_CHILD_PROCESS_IPC_REQUIRED('options.stdio');
  }

  options.execPath = options.execPath || process.execPath;
  options.shell = false;

  return spawn(options.execPath, args, options);
};


exports._forkChild = function _forkChild(fd) {
  // set process.send()
  var p = new Pipe(PipeConstants.IPC);
  p.open(fd);
  p.unref();
  const control = setupChannel(process, p);
  process.on('newListener', function onNewListener(name) {
    if (name === 'message' || name === 'disconnect') control.ref();
  });
  process.on('removeListener', function onRemoveListener(name) {
    if (name === 'message' || name === 'disconnect') control.unref();
  });
};


exports.exec = function exec(/* command , options, callback */) {
  const opts = normalizeExecArgs.apply(null, arguments);
  return exports.execFile(opts.file,
                          opts.options,
                          opts.callback);
};

const customPromiseExecFunction = (orig) => {
  return (...args) => {
    return new Promise((resolve, reject) => {
      orig(...args, (err, stdout, stderr) => {
        if (err !== null) {
          err.stdout = stdout;
          err.stderr = stderr;
          reject(err);
        } else {
          resolve({ stdout, stderr });
        }
      });
    });
  };
};

Object.defineProperty(exports.exec, util.promisify.custom, {
  enumerable: false,
  value: customPromiseExecFunction(exports.exec)
});

exports.execFile = function execFile(file /* , args, options, callback */) {
  var args = [];
  var callback;
  var options = {
    encoding: 'utf8',
    timeout: 0,
    maxBuffer: 200 * 1024,
    killSignal: 'SIGTERM',
    cwd: null,
    env: null,
    shell: false
  };

  // Parse the optional positional parameters.
  var pos = 1;
  if (pos < arguments.length && Array.isArray(arguments[pos])) {
    args = arguments[pos++];
  } else if (pos < arguments.length && arguments[pos] == null) {
    pos++;
  }

  if (pos < arguments.length && typeof arguments[pos] === 'object') {
    util._extend(options, arguments[pos++]);
  } else if (pos < arguments.length && arguments[pos] == null) {
    pos++;
  }

  if (pos < arguments.length && typeof arguments[pos] === 'function') {
    callback = arguments[pos++];
  }

  if (!callback && pos < arguments.length && arguments[pos] != null) {
    throw new ERR_INVALID_ARG_VALUE('args', arguments[pos]);
  }

  // Validate the timeout, if present.
  validateTimeout(options.timeout);

  // Validate maxBuffer, if present.
  validateMaxBuffer(options.maxBuffer);

  options.killSignal = sanitizeKillSignal(options.killSignal);

  var child = spawn(file, args, {
    cwd: options.cwd,
    env: options.env,
    gid: options.gid,
    uid: options.uid,
    shell: options.shell,
    windowsHide: !!options.windowsHide,
    windowsVerbatimArguments: !!options.windowsVerbatimArguments
  });

  var encoding;
  var _stdout = [];
  var _stderr = [];
  if (options.encoding !== 'buffer' && Buffer.isEncoding(options.encoding)) {
    encoding = options.encoding;
  } else {
    encoding = null;
  }
  var stdoutLen = 0;
  var stderrLen = 0;
  var killed = false;
  var exited = false;
  var timeoutId;

  var ex = null;

  var cmd = file;

  function exithandler(code, signal) {
    if (exited) return;
    exited = true;

    if (timeoutId) {
      clearTimeout(timeoutId);
      timeoutId = null;
    }

    if (!callback) return;

    // merge chunks
    var stdout;
    var stderr;
    if (encoding ||
      (
        child.stdout &&
        child.stdout._readableState &&
        child.stdout._readableState.encoding
      )) {
      stdout = _stdout.join('');
    } else {
      stdout = Buffer.concat(_stdout);
    }
    if (encoding ||
      (
        child.stderr &&
        child.stderr._readableState &&
        child.stderr._readableState.encoding
      )) {
      stderr = _stderr.join('');
    } else {
      stderr = Buffer.concat(_stderr);
    }

    if (!ex && code === 0 && signal === null) {
      callback(null, stdout, stderr);
      return;
    }

    if (args.length !== 0)
      cmd += ` ${args.join(' ')}`;

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
      var encoding = child.stdout._readableState.encoding;
      stdoutLen += encoding ? Buffer.byteLength(chunk, encoding) : chunk.length;

      if (stdoutLen > options.maxBuffer) {
        ex = new ERR_CHILD_PROCESS_STDIO_MAXBUFFER('stdout');
        kill();
      } else {
        _stdout.push(chunk);
      }
    });
  }

  if (child.stderr) {
    if (encoding)
      child.stderr.setEncoding(encoding);

    child.stderr.on('data', function onChildStderr(chunk) {
      var encoding = child.stderr._readableState.encoding;
      stderrLen += encoding ? Buffer.byteLength(chunk, encoding) : chunk.length;

      if (stderrLen > options.maxBuffer) {
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
};

Object.defineProperty(exports.execFile, util.promisify.custom, {
  enumerable: false,
  value: customPromiseExecFunction(exports.execFile)
});


exports.spawn = spawn;

function spawnSync(/* file, args, options */) {
  var opts = normalizeSpawnArguments.apply(null, arguments);

  var options = opts.options;

  debug('spawnSync', opts.args, options);

  // Validate the timeout, if present.
  validateTimeout(options.timeout);

  // Validate maxBuffer, if present.
  validateMaxBuffer(options.maxBuffer);

  options.file = opts.file;
  options.args = opts.args;
  options.envPairs = opts.envPairs;

  // Validate and translate the kill signal, if present.
  options.killSignal = sanitizeKillSignal(options.killSignal);

  options.stdio = _validateStdio(options.stdio || 'pipe', true).stdio;

  if (options.input) {
    var stdin = options.stdio[0] = util._extend({}, options.stdio[0]);
    stdin.input = options.input;
  }

  // We may want to pass data in on any given fd, ensure it is a valid buffer
  for (var i = 0; i < options.stdio.length; i++) {
    var input = options.stdio[i] && options.stdio[i].input;
    if (input != null) {
      var pipe = options.stdio[i] = util._extend({}, options.stdio[i]);
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

  return child_process.spawnSync(opts);
}
exports.spawnSync = spawnSync;


function checkExecSyncError(ret, args, cmd) {
  var err;
  if (ret.error) {
    err = ret.error;
  } else if (ret.status !== 0) {
    var msg = 'Command failed: ';
    msg += cmd || args.join(' ');
    if (ret.stderr && ret.stderr.length > 0)
      msg += `\n${ret.stderr.toString()}`;
    // eslint-disable-next-line no-restricted-syntax
    err = new Error(msg);
  }
  if (err) {
    Object.assign(err, ret);
  }
  return err;
}


function execFileSync(/* command, args, options */) {
  var opts = normalizeSpawnArguments.apply(null, arguments);
  var inheritStderr = !opts.options.stdio;

  var ret = spawnSync(opts.file, opts.args.slice(1), opts.options);

  if (inheritStderr && ret.stderr)
    process.stderr.write(ret.stderr);

  var err = checkExecSyncError(ret, opts.args, undefined);

  if (err)
    throw err;

  return ret.stdout;
}
exports.execFileSync = execFileSync;


function execSync(command /* , options */) {
  var opts = normalizeExecArgs.apply(null, arguments);
  var inheritStderr = !opts.options.stdio;

  var ret = spawnSync(opts.file, opts.options);

  if (inheritStderr && ret.stderr)
    process.stderr.write(ret.stderr);

  var err = checkExecSyncError(ret, opts.args, command);

  if (err)
    throw err;

  return ret.stdout;
}
exports.execSync = execSync;

Object.defineProperties(exports, {
  promises: {
    configurable: true,
    enumerable: false,
    get() {
      if (promisesWarn) {
        promises = require('internal/child_process/promises');
        promisesWarn = false;
        process.emitWarning('The child_process.promises API is experimental',
                            'ExperimentalWarning');
      }
      return promises;
    }
  }
});
