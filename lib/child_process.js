'use strict';

const util = require('util');
const internalUtil = require('internal/util');
const debug = util.debuglog('child_process');
const constants = process.binding('constants').os.signals;

const uv = process.binding('uv');
const spawn_sync = process.binding('spawn_sync');
const Buffer = require('buffer').Buffer;
const Pipe = process.binding('pipe_wrap').Pipe;
const { isUint8Array } = process.binding('util');
const child_process = require('internal/child_process');

const errnoException = util._errnoException;
const _validateStdio = child_process._validateStdio;
const setupChannel = child_process.setupChannel;
const ChildProcess = exports.ChildProcess = child_process.ChildProcess;

function stdioStringToArray(option) {
  switch (option) {
    case 'ignore':
    case 'pipe':
    case 'inherit':
      return [option, option, option, 'ipc'];
    default:
      throw new TypeError('Incorrect value of stdio option: ' + option);
  }
}

exports.fork = function(modulePath /*, args, options*/) {

  // Get options and args arguments.
  var execArgv;
  var options = {};
  var args = [];
  var pos = 1;
  if (pos < arguments.length && Array.isArray(arguments[pos])) {
    args = arguments[pos++];
  }

  if (pos < arguments.length && arguments[pos] != null) {
    if (typeof arguments[pos] !== 'object') {
      throw new TypeError('Incorrect value of args option');
    } else {
      options = util._extend({}, arguments[pos++]);
    }
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
    throw new TypeError('Forked processes must have an IPC channel');
  }

  options.execPath = options.execPath || process.execPath;

  return spawn(options.execPath, args, options);
};


exports._forkChild = function(fd) {
  // set process.send()
  var p = new Pipe(true);
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


function normalizeExecArgs(command /*, options, callback*/) {
  let options;
  let callback;

  if (typeof arguments[1] === 'function') {
    options = undefined;
    callback = arguments[1];
  } else {
    options = arguments[1];
    callback = arguments[2];
  }

  // Make a shallow copy so we don't clobber the user's options object.
  options = Object.assign({}, options);
  options.shell = typeof options.shell === 'string' ? options.shell : true;

  return {
    file: command,
    options: options,
    callback: callback
  };
}


exports.exec = function(command /*, options, callback*/) {
  var opts = normalizeExecArgs.apply(null, arguments);
  return exports.execFile(opts.file,
                          opts.options,
                          opts.callback);
};


exports.execFile = function(file /*, args, options, callback*/) {
  var args = [], callback;
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
    options = util._extend(options, arguments[pos++]);
  } else if (pos < arguments.length && arguments[pos] == null) {
    pos++;
  }

  if (pos < arguments.length && typeof arguments[pos] === 'function') {
    callback = arguments[pos++];
  }

  if (!callback && arguments[pos] != null) {
    throw new TypeError('Incorrect value of args option');
  }

  // Validate the timeout, if present.
  validateTimeout(options.timeout);

  // Validate maxBuffer, if present.
  validateMaxBuffer(options.maxBuffer);

  var child = spawn(file, args, {
    cwd: options.cwd,
    env: options.env,
    gid: options.gid,
    uid: options.uid,
    shell: options.shell,
    windowsVerbatimArguments: !!options.windowsVerbatimArguments
  });

  var encoding;
  var _stdout;
  var _stderr;
  if (options.encoding !== 'buffer' && Buffer.isEncoding(options.encoding)) {
    encoding = options.encoding;
    _stdout = '';
    _stderr = '';
  } else {
    _stdout = [];
    _stderr = [];
    encoding = null;
  }
  var stdoutLen = 0;
  var stderrLen = 0;
  var killed = false;
  var exited = false;
  var timeoutId;

  var ex = null;

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
    if (encoding) {
      stdout = _stdout;
      stderr = _stderr;
    } else {
      stdout = Buffer.concat(_stdout);
      stderr = Buffer.concat(_stderr);
    }

    if (ex) {
      // Will be handled later
    } else if (code === 0 && signal === null) {
      callback(null, stdout, stderr);
      return;
    }

    var cmd = file;
    if (args.length !== 0)
      cmd += ' ' + args.join(' ');

    if (!ex) {
      ex = new Error('Command failed: ' + cmd + '\n' + stderr);
      ex.killed = child.killed || killed;
      ex.code = code < 0 ? uv.errname(code) : code;
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
      stdoutLen += encoding ? Buffer.byteLength(chunk, encoding) : chunk.length;

      if (stdoutLen > options.maxBuffer) {
        ex = new Error('stdout maxBuffer exceeded');
        kill();
      } else {
        if (encoding)
          _stdout += chunk;
        else
          _stdout.push(chunk);
      }
    });
  }

  if (child.stderr) {
    if (encoding)
      child.stderr.setEncoding(encoding);

    child.stderr.on('data', function onChildStderr(chunk) {
      stderrLen += encoding ? Buffer.byteLength(chunk, encoding) : chunk.length;

      if (stderrLen > options.maxBuffer) {
        ex = new Error('stderr maxBuffer exceeded');
        kill();
      } else {
        if (encoding)
          _stderr += chunk;
        else
          _stderr.push(chunk);
      }
    });
  }

  child.addListener('close', exithandler);
  child.addListener('error', errorhandler);

  return child;
};

const _deprecatedCustomFds = internalUtil.deprecate(
  function deprecateCustomFds(options) {
    options.stdio = options.customFds.map(function mapCustomFds(fd) {
      return fd === -1 ? 'pipe' : fd;
    });
  }, 'child_process: options.customFds option is deprecated. ' +
     'Use options.stdio instead.', 'DEP0006');

function _convertCustomFds(options) {
  if (options.customFds && !options.stdio) {
    _deprecatedCustomFds(options);
  }
}

function normalizeSpawnArguments(file /*, args, options*/) {
  var args, options;

  if (typeof file !== 'string' || file.length === 0)
    throw new TypeError('"file" argument must be a non-empty string');

  if (Array.isArray(arguments[1])) {
    args = arguments[1].slice(0);
    options = arguments[2];
  } else if (arguments[1] !== undefined &&
             (arguments[1] === null || typeof arguments[1] !== 'object')) {
    throw new TypeError('Incorrect value of args option');
  } else {
    args = [];
    options = arguments[1];
  }

  if (options === undefined)
    options = {};
  else if (options === null || typeof options !== 'object')
    throw new TypeError('"options" argument must be an object');

  // Validate the cwd, if present.
  if (options.cwd != null &&
      typeof options.cwd !== 'string') {
    throw new TypeError('"cwd" must be a string');
  }

  // Validate detached, if present.
  if (options.detached != null &&
      typeof options.detached !== 'boolean') {
    throw new TypeError('"detached" must be a boolean');
  }

  // Validate the uid, if present.
  if (options.uid != null && !Number.isInteger(options.uid)) {
    throw new TypeError('"uid" must be an integer');
  }

  // Validate the gid, if present.
  if (options.gid != null && !Number.isInteger(options.gid)) {
    throw new TypeError('"gid" must be an integer');
  }

  // Validate the shell, if present.
  if (options.shell != null &&
      typeof options.shell !== 'boolean' &&
      typeof options.shell !== 'string') {
    throw new TypeError('"shell" must be a boolean or string');
  }

  // Validate argv0, if present.
  if (options.argv0 != null &&
      typeof options.argv0 !== 'string') {
    throw new TypeError('"argv0" must be a string');
  }

  // Validate windowsVerbatimArguments, if present.
  if (options.windowsVerbatimArguments != null &&
      typeof options.windowsVerbatimArguments !== 'boolean') {
    throw new TypeError('"windowsVerbatimArguments" must be a boolean');
  }

  // Make a shallow copy so we don't clobber the user's options object.
  options = Object.assign({}, options);

  if (options.shell) {
    const command = [file].concat(args).join(' ');

    if (process.platform === 'win32') {
      file = typeof options.shell === 'string' ? options.shell :
              process.env.comspec || 'cmd.exe';
      args = ['/d', '/s', '/c', '"' + command + '"'];
      options.windowsVerbatimArguments = true;
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
    args.unshift(options.argv0);
  } else {
    args.unshift(file);
  }

  var env = options.env || process.env;
  var envPairs = [];

  for (var key in env) {
    envPairs.push(key + '=' + env[key]);
  }

  _convertCustomFds(options);

  return {
    file: file,
    args: args,
    options: options,
    envPairs: envPairs
  };
}


var spawn = exports.spawn = function(/*file, args, options*/) {
  var opts = normalizeSpawnArguments.apply(null, arguments);
  var options = opts.options;
  var child = new ChildProcess();

  debug('spawn', opts.args, options);

  child.spawn({
    file: opts.file,
    args: opts.args,
    cwd: options.cwd,
    windowsVerbatimArguments: !!options.windowsVerbatimArguments,
    detached: !!options.detached,
    envPairs: opts.envPairs,
    stdio: options.stdio,
    uid: options.uid,
    gid: options.gid
  });

  return child;
};


function lookupSignal(signal) {
  if (typeof signal === 'number')
    return signal;

  if (!(signal in constants))
    throw new Error('Unknown signal: ' + signal);

  return constants[signal];
}


function spawnSync(/*file, args, options*/) {
  var opts = normalizeSpawnArguments.apply(null, arguments);

  var options = opts.options;

  var i;

  debug('spawnSync', opts.args, options);

  // Validate the timeout, if present.
  validateTimeout(options.timeout);

  // Validate maxBuffer, if present.
  validateMaxBuffer(options.maxBuffer);

  options.file = opts.file;
  options.args = opts.args;
  options.envPairs = opts.envPairs;

  // Validate and translate the kill signal, if present.
  options.killSignal = validateKillSignal(options.killSignal);

  options.stdio = _validateStdio(options.stdio || 'pipe', true).stdio;

  if (options.input) {
    var stdin = options.stdio[0] = util._extend({}, options.stdio[0]);
    stdin.input = options.input;
  }

  // We may want to pass data in on any given fd, ensure it is a valid buffer
  for (i = 0; i < options.stdio.length; i++) {
    var input = options.stdio[i] && options.stdio[i].input;
    if (input != null) {
      var pipe = options.stdio[i] = util._extend({}, options.stdio[i]);
      if (isUint8Array(input))
        pipe.input = input;
      else if (typeof input === 'string')
        pipe.input = Buffer.from(input, options.encoding);
      else
        throw new TypeError(util.format(
            'stdio[%d] should be Buffer, Uint8Array or string not %s',
            i,
            typeof input));
    }
  }

  var result = spawn_sync.spawn(options);

  if (result.output && options.encoding && options.encoding !== 'buffer') {
    for (i = 0; i < result.output.length; i++) {
      if (!result.output[i])
        continue;
      result.output[i] = result.output[i].toString(options.encoding);
    }
  }

  result.stdout = result.output && result.output[1];
  result.stderr = result.output && result.output[2];

  if (result.error) {
    result.error = errnoException(result.error, 'spawnSync ' + opts.file);
    result.error.path = opts.file;
    result.error.spawnargs = opts.args.slice(1);
  }

  util._extend(result, opts);

  return result;
}
exports.spawnSync = spawnSync;


function checkExecSyncError(ret) {
  if (ret.error || ret.status !== 0) {
    var err = ret.error;
    ret.error = null;

    if (!err) {
      var msg = 'Command failed: ';
      msg += ret.cmd || ret.args.join(' ');
      if (ret.stderr && ret.stderr.length > 0)
        msg += '\n' + ret.stderr.toString();
      err = new Error(msg);
    }

    util._extend(err, ret);
    return err;
  }

  return false;
}


function execFileSync(/*command, args, options*/) {
  var opts = normalizeSpawnArguments.apply(null, arguments);
  var inheritStderr = !opts.options.stdio;

  var ret = spawnSync(opts.file, opts.args.slice(1), opts.options);

  if (inheritStderr && ret.stderr)
    process.stderr.write(ret.stderr);

  var err = checkExecSyncError(ret);

  if (err)
    throw err;
  else
    return ret.stdout;
}
exports.execFileSync = execFileSync;


function execSync(command /*, options*/) {
  var opts = normalizeExecArgs.apply(null, arguments);
  var inheritStderr = !opts.options.stdio;

  var ret = spawnSync(opts.file, opts.options);
  ret.cmd = command;

  if (inheritStderr && ret.stderr)
    process.stderr.write(ret.stderr);

  var err = checkExecSyncError(ret);

  if (err)
    throw err;
  else
    return ret.stdout;
}
exports.execSync = execSync;


function validateTimeout(timeout) {
  if (timeout != null && !(Number.isInteger(timeout) && timeout >= 0)) {
    throw new TypeError('"timeout" must be an unsigned integer');
  }
}


function validateMaxBuffer(maxBuffer) {
  if (maxBuffer != null && !(typeof maxBuffer === 'number' && maxBuffer >= 0)) {
    throw new TypeError('"maxBuffer" must be a positive number');
  }
}


function validateKillSignal(killSignal) {
  if (typeof killSignal === 'string' || typeof killSignal === 'number') {
    killSignal = lookupSignal(killSignal);

    if (killSignal === 0)
      throw new RangeError('"killSignal" cannot be 0');
  } else if (killSignal != null) {
    throw new TypeError('"killSignal" must be a string or number');
  }

  return killSignal;
}
