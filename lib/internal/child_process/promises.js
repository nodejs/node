'use strict';

const util = require('util');
const { getSystemErrorName } = require('internal/util');
const { Buffer } = require('buffer');
const {
  ERR_CHILD_PROCESS_STDIO_MAXBUFFER,
} = require('internal/errors').codes;
const {
  normalizeExecArgs,
  sanitizeKillSignal,
  spawn,
  validateMaxBuffer,
  validateTimeout
} = require('internal/child_process/utils');

async function exec() {
  const opts = normalizeExecArgs.apply(null, arguments);
  return execFile(opts.file, opts.options);
}

async function execFile(file /* , args, options */) {
  return new Promise((resolve, reject) => {
    var args = [];
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
        resolve({ stdout, stderr });
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
      ex.stdout = stdout;
      ex.stderr = stderr;
      reject(ex);
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
        stdoutLen +=
          encoding ? Buffer.byteLength(chunk, encoding) : chunk.length;

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
        stderrLen +=
          encoding ? Buffer.byteLength(chunk, encoding) : chunk.length;

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
  });
}


module.exports = {
  exec,
  execFile
};
