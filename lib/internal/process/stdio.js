'use strict';

const {
  ERR_STDERR_CLOSE,
  ERR_STDOUT_CLOSE,
  ERR_UNKNOWN_STDIN_TYPE,
  ERR_UNKNOWN_STREAM_TYPE
} = require('internal/errors').codes;

exports.setupProcessStdio = setupProcessStdio;
exports.getMainThreadStdio = getMainThreadStdio;

function getMainThreadStdio() {
  var stdin;
  var stdout;
  var stderr;

  function getStdout() {
    if (stdout) return stdout;
    stdout = createWritableStdioStream(1);
    stdout.destroySoon = stdout.destroy;
    stdout._destroy = function(er, cb) {
      // Avoid errors if we already emitted
      er = er || new ERR_STDOUT_CLOSE();
      cb(er);
    };
    if (stdout.isTTY) {
      process.on('SIGWINCH', () => stdout._refreshSize());
    }
    return stdout;
  }

  function getStderr() {
    if (stderr) return stderr;
    stderr = createWritableStdioStream(2);
    stderr.destroySoon = stderr.destroy;
    stderr._destroy = function(er, cb) {
      // Avoid errors if we already emitted
      er = er || new ERR_STDERR_CLOSE();
      cb(er);
    };
    if (stderr.isTTY) {
      process.on('SIGWINCH', () => stderr._refreshSize());
    }
    return stderr;
  }

  function getStdin() {
    if (stdin) return stdin;
    const tty_wrap = process.binding('tty_wrap');
    const fd = 0;

    switch (tty_wrap.guessHandleType(fd)) {
      case 'TTY':
        var tty = require('tty');
        stdin = new tty.ReadStream(fd, {
          highWaterMark: 0,
          readable: true,
          writable: false
        });
        break;

      case 'FILE':
        var fs = require('fs');
        stdin = new fs.ReadStream(null, { fd: fd, autoClose: false });
        break;

      case 'PIPE':
      case 'TCP':
        var net = require('net');

        // It could be that process has been started with an IPC channel
        // sitting on fd=0, in such case the pipe for this fd is already
        // present and creating a new one will lead to the assertion failure
        // in libuv.
        if (process.channel && process.channel.fd === fd) {
          stdin = new net.Socket({
            handle: process.channel,
            readable: true,
            writable: false,
            manualStart: true
          });
        } else {
          stdin = new net.Socket({
            fd: fd,
            readable: true,
            writable: false,
            manualStart: true
          });
        }
        // Make sure the stdin can't be `.end()`-ed
        stdin._writableState.ended = true;
        break;

      default:
        // Probably an error on in uv_guess_handle()
        throw new ERR_UNKNOWN_STDIN_TYPE();
    }

    // For supporting legacy API we put the FD here.
    stdin.fd = fd;

    // stdin starts out life in a paused state, but node doesn't
    // know yet.  Explicitly to readStop() it to put it in the
    // not-reading state.
    if (stdin._handle && stdin._handle.readStop) {
      stdin._handle.reading = false;
      stdin._readableState.reading = false;
      stdin._handle.readStop();
    }

    // If the user calls stdin.pause(), then we need to stop reading
    // once the stream implementation does so (one nextTick later),
    // so that the process can close down.
    stdin.on('pause', () => {
      process.nextTick(onpause);
    });

    function onpause() {
      if (!stdin._handle)
        return;
      if (stdin._handle.reading && !stdin._readableState.flowing) {
        stdin._readableState.reading = false;
        stdin._handle.reading = false;
        stdin._handle.readStop();
      }
    }

    return stdin;
  }

  return {
    getStdout,
    getStderr,
    getStdin
  };
}

function setupProcessStdio({ getStdout, getStdin, getStderr }) {
  Object.defineProperty(process, 'stdout', {
    configurable: true,
    enumerable: true,
    get: getStdout
  });

  Object.defineProperty(process, 'stderr', {
    configurable: true,
    enumerable: true,
    get: getStderr
  });

  Object.defineProperty(process, 'stdin', {
    configurable: true,
    enumerable: true,
    get: getStdin
  });

  process.openStdin = function() {
    process.stdin.resume();
    return process.stdin;
  };
}

function createWritableStdioStream(fd) {
  var stream;
  const tty_wrap = process.binding('tty_wrap');

  // Note stream._type is used for test-module-load-list.js

  switch (tty_wrap.guessHandleType(fd)) {
    case 'TTY':
      var tty = require('tty');
      stream = new tty.WriteStream(fd);
      stream._type = 'tty';
      break;

    case 'FILE':
      const SyncWriteStream = require('internal/fs/sync_write_stream');
      stream = new SyncWriteStream(fd, { autoClose: false });
      stream._type = 'fs';
      break;

    case 'PIPE':
    case 'TCP':
      var net = require('net');
      stream = new net.Socket({
        fd: fd,
        readable: false,
        writable: true
      });
      stream._type = 'pipe';
      break;

    default:
      // Probably an error on in uv_guess_handle()
      throw new ERR_UNKNOWN_STREAM_TYPE();
  }

  // For supporting legacy API we put the FD here.
  stream.fd = fd;

  stream._isStdio = true;

  return stream;
}
