'use strict';

const util = require('util');
const internalUtil = require('internal/util');
const debug = util.debuglog('child_process');
const constants = require('constants');

const uv = process.binding('uv');
const spawn_sync = process.binding('spawn_sync');
const Buffer = require('buffer').Buffer;
const Pipe = process.binding('pipe_wrap').Pipe;
const child_process = require('internal/child_process');

const errnoException = util._errnoException;
const _validateStdio = child_process._validateStdio;
const setupChannel = child_process.setupChannel;
const ChildProcess = exports.ChildProcess = child_process.ChildProcess;

exports.fork = function(modulePath /*, args, options*/) {

  // Get options and args arguments.
  var options, args, execArgv;
  if (Array.isArray(arguments[1])) {
    args = arguments[1];
    options = util._extend({}, arguments[2]);
  } else if (arguments[1] && typeof arguments[1] !== 'object') {
    throw new TypeError('Incorrect value of args option');
  } else {
    args = [];
    options = util._extend({}, arguments[1]);
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

  // Leave stdin open for the IPC channel. stdout and stderr should be the
  // same as the parent's if silent isn't set.
  options.stdio = options.silent ? ['pipe', 'pipe', 'pipe', 'ipc'] :
      [0, 1, 2, 'ipc'];

  options.execPath = options.execPath || process.execPath;

  return spawn(options.execPath, args, options);
};


exports._forkChild = function(fd) {
  // set process.send()
  var p = new Pipe(true);
  p.open(fd);
  p.unref();
  const control = setupChannel(process, p);
  process.on('newListener', function(name) {
    if (name === 'message' || name === 'disconnect') control.ref();
  });
  process.on('removeListener', function(name) {
    if (name === 'message' || name === 'disconnect') control.unref();
  });
};


function normalizeExecArgs(command /*, options, callback*/) {
  var file, args, options, callback;

  if (typeof arguments[1] === 'function') {
    options = undefined;
    callback = arguments[1];
  } else {
    options = arguments[1];
    callback = arguments[2];
  }

  if (process.platform === 'win32') {
    file = process.env.comspec || 'cmd.exe';
    args = ['/s', '/c', '"' + command + '"'];
    // Make a shallow copy before patching so we don't clobber the user's
    // options object.
    options = util._extend({}, options);
    options.windowsVerbatimArguments = true;
  } else {
    file = '/bin/sh';
    args = ['-c', command];
  }

  if (options && options.shell)
    file = options.shell;

  return {
    cmd: command,
    file: file,
    args: args,
    options: options,
    callback: callback
  };
}


exports.exec = function(command /*, options, callback*/) {
  var opts = normalizeExecArgs.apply(null, arguments);
  return exports.execFile(opts.file,
                          opts.args,
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
    env: null
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

  if (pos === 1 && arguments.length > 1) {
    throw new TypeError('Incorrect value of args option');
  }

  var child = spawn(file, args, {
    cwd: options.cwd,
    env: options.env,
    gid: options.gid,
    uid: options.uid,
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
    if (!encoding) {
      stdout = Buffer.concat(_stdout);
      stderr = Buffer.concat(_stderr);
    } else {
      stdout = _stdout;
      stderr = _stderr;
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
    timeoutId = setTimeout(function() {
      kill();
      timeoutId = null;
    }, options.timeout);
  }

  if (child.stdout) {
    if (encoding)
      child.stdout.setEncoding(encoding);

    child.stdout.addListener('data', function(chunk) {
      stdoutLen += chunk.length;

      if (stdoutLen > options.maxBuffer) {
        ex = new Error('stdout maxBuffer exceeded');
        kill();
      } else {
        if (!encoding)
          _stdout.push(chunk);
        else
          _stdout += chunk;
      }
    });
  }

  if (child.stderr) {
    if (encoding)
      child.stderr.setEncoding(encoding);

    child.stderr.addListener('data', function(chunk) {
      stderrLen += chunk.length;

      if (stderrLen > options.maxBuffer) {
        ex = new Error('stderr maxBuffer exceeded');
        kill();
      } else {
        if (!encoding)
          _stderr.push(chunk);
        else
          _stderr += chunk;
      }
    });
  }

  child.addListener('close', exithandler);
  child.addListener('error', errorhandler);

  return child;
};

var _deprecatedCustomFds = internalUtil.deprecate(function(options) {
  options.stdio = options.customFds.map(function(fd) {
    return fd === -1 ? 'pipe' : fd;
  });
}, 'child_process: options.customFds option is deprecated. ' +
   'Use options.stdio instead.');

function _convertCustomFds(options) {
  if (options && options.customFds && !options.stdio) {
    _deprecatedCustomFds(options);
  }
}

function normalizeSpawnArguments(file /*, args, options*/) {
  var args, options;

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
    throw new TypeError('options argument must be an object');

  options = util._extend({}, options);
  args.unshift(file);

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

  options.file = opts.file;
  options.args = opts.args;
  options.envPairs = opts.envPairs;

  if (options.killSignal)
    options.killSignal = lookupSignal(options.killSignal);

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
      if (Buffer.isBuffer(input))
        pipe.input = input;
      else if (typeof input === 'string')
        pipe.input = new Buffer(input, options.encoding);
      else
        throw new TypeError(util.format(
            'stdio[%d] should be Buffer or string not %s',
            i,
            typeof input));
    }
  }

  var result = spawn_sync.spawn(options);

  if (result.output && options.encoding) {
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
      var msg = 'Command failed: ' +
                (ret.cmd ? ret.cmd : ret.args.join(' ')) +
                (ret.stderr ? '\n' + ret.stderr.toString() : '');
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

  if (inheritStderr)
    process.stderr.write(ret.stderr);

  var err = checkExecSyncError(ret);

  if (err)
    throw err;
  else
    return ret.stdout;
}
exports.execFileSync = execFileSync;


function execSync(/*command, options*/) {
  var opts = normalizeExecArgs.apply(null, arguments);
  var inheritStderr = opts.options ? !opts.options.stdio : true;

  var ret = spawnSync(opts.file, opts.args, opts.options);
  ret.cmd = opts.cmd;

  if (inheritStderr)
    process.stderr.write(ret.stderr);

  var err = checkExecSyncError(ret);

  if (err)
    throw err;
  else
    return ret.stdout;
}
exports.execSync = execSync;
