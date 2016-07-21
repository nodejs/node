/* eslint-disable required-modules */
'use strict';
var path = require('path');
var fs = require('fs');
var assert = require('assert');
var os = require('os');
var child_process = require('child_process');
const stream = require('stream');
const util = require('util');
const Timer = process.binding('timer_wrap').Timer;

const testRoot = process.env.NODE_TEST_DIR ?
                   path.resolve(process.env.NODE_TEST_DIR) : __dirname;

exports.testDir = __dirname;
exports.fixturesDir = path.join(exports.testDir, 'fixtures');
exports.libDir = path.join(exports.testDir, '../lib');
exports.tmpDirName = 'tmp';
exports.PORT = +process.env.NODE_COMMON_PORT || 12346;
exports.isWindows = process.platform === 'win32';
exports.isWOW64 = exports.isWindows &&
                  (process.env.PROCESSOR_ARCHITEW6432 !== undefined);
exports.isAix = process.platform === 'aix';
exports.isLinuxPPCBE = (process.platform === 'linux') &&
                       (process.arch === 'ppc64') &&
                       (os.endianness() === 'BE');
exports.isSunOS = process.platform === 'sunos';
exports.isFreeBSD = process.platform === 'freebsd';
exports.isLinux = process.platform === 'linux';
exports.isOSX = process.platform === 'darwin';

exports.enoughTestMem = os.totalmem() > 0x40000000; /* 1 Gb */
exports.rootDir = exports.isWindows ? 'c:\\' : '/';

function rimrafSync(p) {
  try {
    var st = fs.lstatSync(p);
  } catch (e) {
    if (e.code === 'ENOENT')
      return;
  }

  try {
    if (st && st.isDirectory())
      rmdirSync(p, null);
    else
      fs.unlinkSync(p);
  } catch (e) {
    if (e.code === 'ENOENT')
      return;
    if (e.code === 'EPERM')
      return rmdirSync(p, e);
    if (e.code !== 'EISDIR')
      throw e;
    rmdirSync(p, e);
  }
}

function rmdirSync(p, originalEr) {
  try {
    fs.rmdirSync(p);
  } catch (e) {
    if (e.code === 'ENOTDIR')
      throw originalEr;
    if (e.code === 'ENOTEMPTY' || e.code === 'EEXIST' || e.code === 'EPERM') {
      const enc = exports.isLinux ? 'buffer' : 'utf8';
      fs.readdirSync(p, enc).forEach((f) => {
        if (f instanceof Buffer) {
          const buf = Buffer.concat([Buffer.from(p), Buffer.from(path.sep), f]);
          rimrafSync(buf);
        } else {
          rimrafSync(path.join(p, f));
        }
      });
      fs.rmdirSync(p);
    }
  }
}

exports.refreshTmpDir = function() {
  rimrafSync(exports.tmpDir);
  fs.mkdirSync(exports.tmpDir);
};

if (process.env.TEST_THREAD_ID) {
  exports.PORT += process.env.TEST_THREAD_ID * 100;
  exports.tmpDirName += '.' + process.env.TEST_THREAD_ID;
}
exports.tmpDir = path.join(testRoot, exports.tmpDirName);

var opensslCli = null;
var inFreeBSDJail = null;
var localhostIPv4 = null;

exports.localIPv6Hosts = ['localhost'];
if (exports.isLinux) {
  exports.localIPv6Hosts = [
    // Debian/Ubuntu
    'ip6-localhost',
    'ip6-loopback',

    // SUSE
    'ipv6-localhost',
    'ipv6-loopback',

    // Typically universal
    'localhost',
  ];
}

Object.defineProperty(exports, 'inFreeBSDJail', {
  get: function() {
    if (inFreeBSDJail !== null) return inFreeBSDJail;

    if (exports.isFreeBSD &&
      child_process.execSync('sysctl -n security.jail.jailed').toString() ===
      '1\n') {
      inFreeBSDJail = true;
    } else {
      inFreeBSDJail = false;
    }
    return inFreeBSDJail;
  }
});

Object.defineProperty(exports, 'localhostIPv4', {
  get: function() {
    if (localhostIPv4 !== null) return localhostIPv4;

    if (exports.inFreeBSDJail) {
      // Jailed network interfaces are a bit special - since we need to jump
      // through loops, as well as this being an exception case, assume the
      // user will provide this instead.
      if (process.env.LOCALHOST) {
        localhostIPv4 = process.env.LOCALHOST;
      } else {
        console.error('Looks like we\'re in a FreeBSD Jail. ' +
                      'Please provide your default interface address ' +
                      'as LOCALHOST or expect some tests to fail.');
      }
    }

    if (localhostIPv4 === null) localhostIPv4 = '127.0.0.1';

    return localhostIPv4;
  }
});

// opensslCli defined lazily to reduce overhead of spawnSync
Object.defineProperty(exports, 'opensslCli', {get: function() {
  if (opensslCli !== null) return opensslCli;

  if (process.config.variables.node_shared_openssl) {
    // use external command
    opensslCli = 'openssl';
  } else {
    // use command built from sources included in Node.js repository
    opensslCli = path.join(path.dirname(process.execPath), 'openssl-cli');
  }

  if (exports.isWindows) opensslCli += '.exe';

  var openssl_cmd = child_process.spawnSync(opensslCli, ['version']);
  if (openssl_cmd.status !== 0 || openssl_cmd.error !== undefined) {
    // openssl command cannot be executed
    opensslCli = false;
  }
  return opensslCli;
}, enumerable: true});

Object.defineProperty(exports, 'hasCrypto', {
  get: function() {
    return process.versions.openssl ? true : false;
  }
});

Object.defineProperty(exports, 'hasFipsCrypto', {
  get: function() {
    return exports.hasCrypto && require('crypto').fips;
  }
});

if (exports.isWindows) {
  exports.PIPE = '\\\\.\\pipe\\libuv-test';
  if (process.env.TEST_THREAD_ID) {
    exports.PIPE += '.' + process.env.TEST_THREAD_ID;
  }
} else {
  exports.PIPE = exports.tmpDir + '/test.sock';
}

if (exports.isWindows) {
  exports.faketimeCli = false;
} else {
  exports.faketimeCli = path.join(__dirname, '..', 'tools', 'faketime', 'src',
                                  'faketime');
}

var ifaces = os.networkInterfaces();
exports.hasIPv6 = Object.keys(ifaces).some(function(name) {
  return /lo/.test(name) && ifaces[name].some(function(info) {
    return info.family === 'IPv6';
  });
});


exports.ddCommand = function(filename, kilobytes) {
  if (exports.isWindows) {
    var p = path.resolve(exports.fixturesDir, 'create-file.js');
    return '"' + process.argv[0] + '" "' + p + '" "' +
           filename + '" ' + (kilobytes * 1024);
  } else {
    return 'dd if=/dev/zero of="' + filename + '" bs=1024 count=' + kilobytes;
  }
};


exports.spawnCat = function(options) {
  var spawn = require('child_process').spawn;

  if (exports.isWindows) {
    return spawn('more', [], options);
  } else {
    return spawn('cat', [], options);
  }
};


exports.spawnSyncCat = function(options) {
  var spawnSync = require('child_process').spawnSync;

  if (exports.isWindows) {
    return spawnSync('more', [], options);
  } else {
    return spawnSync('cat', [], options);
  }
};


exports.spawnPwd = function(options) {
  var spawn = require('child_process').spawn;

  if (exports.isWindows) {
    return spawn('cmd.exe', ['/c', 'cd'], options);
  } else {
    return spawn('pwd', [], options);
  }
};


exports.spawnSyncPwd = function(options) {
  const spawnSync = require('child_process').spawnSync;

  if (exports.isWindows) {
    return spawnSync('cmd.exe', ['/c', 'cd'], options);
  } else {
    return spawnSync('pwd', [], options);
  }
};

exports.platformTimeout = function(ms) {
  if (process.config.target_defaults.default_configuration === 'Debug')
    ms = 2 * ms;

  if (exports.isAix)
    return 2 * ms; // default localhost speed is slower on AIX

  if (process.arch !== 'arm')
    return ms;

  const armv = process.config.variables.arm_version;

  if (armv === '6')
    return 7 * ms;  // ARMv6

  if (armv === '7')
    return 2 * ms;  // ARMv7

  return ms; // ARMv8+
};

var knownGlobals = [setTimeout,
                    setInterval,
                    setImmediate,
                    clearTimeout,
                    clearInterval,
                    clearImmediate,
                    console,
                    constructor, // Enumerable in V8 3.21.
                    Buffer,
                    process,
                    global];

if (global.gc) {
  knownGlobals.push(global.gc);
}

if (global.DTRACE_HTTP_SERVER_RESPONSE) {
  knownGlobals.push(DTRACE_HTTP_SERVER_RESPONSE);
  knownGlobals.push(DTRACE_HTTP_SERVER_REQUEST);
  knownGlobals.push(DTRACE_HTTP_CLIENT_RESPONSE);
  knownGlobals.push(DTRACE_HTTP_CLIENT_REQUEST);
  knownGlobals.push(DTRACE_NET_STREAM_END);
  knownGlobals.push(DTRACE_NET_SERVER_CONNECTION);
}

if (global.COUNTER_NET_SERVER_CONNECTION) {
  knownGlobals.push(COUNTER_NET_SERVER_CONNECTION);
  knownGlobals.push(COUNTER_NET_SERVER_CONNECTION_CLOSE);
  knownGlobals.push(COUNTER_HTTP_SERVER_REQUEST);
  knownGlobals.push(COUNTER_HTTP_SERVER_RESPONSE);
  knownGlobals.push(COUNTER_HTTP_CLIENT_REQUEST);
  knownGlobals.push(COUNTER_HTTP_CLIENT_RESPONSE);
}

if (global.LTTNG_HTTP_SERVER_RESPONSE) {
  knownGlobals.push(LTTNG_HTTP_SERVER_RESPONSE);
  knownGlobals.push(LTTNG_HTTP_SERVER_REQUEST);
  knownGlobals.push(LTTNG_HTTP_CLIENT_RESPONSE);
  knownGlobals.push(LTTNG_HTTP_CLIENT_REQUEST);
  knownGlobals.push(LTTNG_NET_STREAM_END);
  knownGlobals.push(LTTNG_NET_SERVER_CONNECTION);
}

if (global.ArrayBuffer) {
  knownGlobals.push(ArrayBuffer);
  knownGlobals.push(Int8Array);
  knownGlobals.push(Uint8Array);
  knownGlobals.push(Uint8ClampedArray);
  knownGlobals.push(Int16Array);
  knownGlobals.push(Uint16Array);
  knownGlobals.push(Int32Array);
  knownGlobals.push(Uint32Array);
  knownGlobals.push(Float32Array);
  knownGlobals.push(Float64Array);
  knownGlobals.push(DataView);
}

// Harmony features.
if (global.Proxy) {
  knownGlobals.push(Proxy);
}

if (global.Symbol) {
  knownGlobals.push(Symbol);
}

function allowGlobals(...whitelist) {
  knownGlobals = knownGlobals.concat(whitelist);
}
exports.allowGlobals = allowGlobals;

function leakedGlobals() {
  var leaked = [];

  for (var val in global)
    if (-1 === knownGlobals.indexOf(global[val]))
      leaked.push(val);

  return leaked;
}
exports.leakedGlobals = leakedGlobals;

// Turn this off if the test should not check for global leaks.
exports.globalCheck = true;

process.on('exit', function() {
  if (!exports.globalCheck) return;
  var leaked = leakedGlobals();
  if (leaked.length > 0) {
    console.error('Unknown globals: %s', leaked);
    assert.ok(false, 'Unknown global found');
  }
});


var mustCallChecks = [];


function runCallChecks(exitCode) {
  if (exitCode !== 0) return;

  var failed = mustCallChecks.filter(function(context) {
    return context.actual !== context.expected;
  });

  failed.forEach(function(context) {
    console.log('Mismatched %s function calls. Expected %d, actual %d.',
                context.name,
                context.expected,
                context.actual);
    console.log(context.stack.split('\n').slice(2).join('\n'));
  });

  if (failed.length) process.exit(1);
}


exports.mustCall = function(fn, expected) {
  if (typeof expected !== 'number') expected = 1;

  var context = {
    expected: expected,
    actual: 0,
    stack: (new Error()).stack,
    name: fn.name || '<anonymous>'
  };

  // add the exit listener only once to avoid listener leak warnings
  if (mustCallChecks.length === 0) process.on('exit', runCallChecks);

  mustCallChecks.push(context);

  return function() {
    context.actual++;
    return fn.apply(this, arguments);
  };
};

exports.hasMultiLocalhost = function hasMultiLocalhost() {
  var TCP = process.binding('tcp_wrap').TCP;
  var t = new TCP();
  var ret = t.bind('127.0.0.2', exports.PORT);
  t.close();
  return ret === 0;
};

exports.fileExists = function(pathname) {
  try {
    fs.accessSync(pathname);
    return true;
  } catch (err) {
    return false;
  }
};

exports.fail = function(msg) {
  assert.fail(null, null, msg);
};

exports.skip = function(msg) {
  console.log(`1..0 # Skipped: ${msg}`);
};

// A stream to push an array into a REPL
function ArrayStream() {
  this.run = function(data) {
    data.forEach((line) => {
      this.emit('data', line + '\n');
    });
  };
}

util.inherits(ArrayStream, stream.Stream);
exports.ArrayStream = ArrayStream;
ArrayStream.prototype.readable = true;
ArrayStream.prototype.writable = true;
ArrayStream.prototype.pause = function() {};
ArrayStream.prototype.resume = function() {};
ArrayStream.prototype.write = function() {};

// Returns true if the exit code "exitCode" and/or signal name "signal"
// represent the exit code and/or signal name of a node process that aborted,
// false otherwise.
exports.nodeProcessAborted = function nodeProcessAborted(exitCode, signal) {
  // Depending on the compiler used, node will exit with either
  // exit code 132 (SIGILL), 133 (SIGTRAP) or 134 (SIGABRT).
  var expectedExitCodes = [132, 133, 134];

  // On platforms using KSH as the default shell (like SmartOS),
  // when a process aborts, KSH exits with an exit code that is
  // greater than 256, and thus the exit code emitted with the 'exit'
  // event is null and the signal is set to either SIGILL, SIGTRAP,
  // or SIGABRT (depending on the compiler).
  const expectedSignals = ['SIGILL', 'SIGTRAP', 'SIGABRT'];

  // On Windows, v8's base::OS::Abort triggers an access violation,
  // which corresponds to exit code 3221225477 (0xC0000005)
  if (exports.isWindows)
    expectedExitCodes = [3221225477];

  // When using --abort-on-uncaught-exception, V8 will use
  // base::OS::Abort to terminate the process.
  // Depending on the compiler used, the shell or other aspects of
  // the platform used to build the node binary, this will actually
  // make V8 exit by aborting or by raising a signal. In any case,
  // one of them (exit code or signal) needs to be set to one of
  // the expected exit codes or signals.
  if (signal !== null) {
    return expectedSignals.indexOf(signal) > -1;
  } else {
    return expectedExitCodes.indexOf(exitCode) > -1;
  }
};

exports.busyLoop = function busyLoop(time) {
  var startTime = Timer.now();
  var stopTime = startTime + time;
  while (Timer.now() < stopTime) {}
};
