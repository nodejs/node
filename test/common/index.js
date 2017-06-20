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

/* eslint-disable required-modules, crypto-check */
'use strict';
const path = require('path');
const fs = require('fs');
const assert = require('assert');
const os = require('os');
const { exec, execSync, spawn, spawnSync } = require('child_process');
const stream = require('stream');
const util = require('util');
const Timer = process.binding('timer_wrap').Timer;
const { fixturesDir } = require('./fixtures');

const testRoot = process.env.NODE_TEST_DIR ?
  fs.realpathSync(process.env.NODE_TEST_DIR) : path.resolve(__dirname, '..');

const noop = () => {};

exports.fixturesDir = fixturesDir;

exports.tmpDirName = 'tmp';
// PORT should match the definition in test/testpy/__init__.py.
exports.PORT = +process.env.NODE_COMMON_PORT || 12346;
exports.isWindows = process.platform === 'win32';
exports.isWOW64 = exports.isWindows &&
                  (process.env.PROCESSOR_ARCHITEW6432 !== undefined);
exports.isAIX = process.platform === 'aix';
exports.isLinuxPPCBE = (process.platform === 'linux') &&
                       (process.arch === 'ppc64') &&
                       (os.endianness() === 'BE');
exports.isSunOS = process.platform === 'sunos';
exports.isFreeBSD = process.platform === 'freebsd';
exports.isLinux = process.platform === 'linux';
exports.isOSX = process.platform === 'darwin';

exports.enoughTestMem = os.totalmem() > 0x40000000; /* 1 Gb */
const cpus = os.cpus();
exports.enoughTestCpu = Array.isArray(cpus) &&
                        (cpus.length > 1 || cpus[0].speed > 999);

exports.rootDir = exports.isWindows ? 'c:\\' : '/';
exports.buildType = process.config.target_defaults.default_configuration;

// If env var is set then enable async_hook hooks for all tests.
if (process.env.NODE_TEST_WITH_ASYNC_HOOKS) {
  const destroydIdsList = {};
  const destroyListList = {};
  const initHandles = {};
  const async_wrap = process.binding('async_wrap');

  process.on('exit', () => {
    // itterate through handles to make sure nothing crashes
    for (const k in initHandles)
      util.inspect(initHandles[k]);
  });

  const _addIdToDestroyList = async_wrap.addIdToDestroyList;
  async_wrap.addIdToDestroyList = function addIdToDestroyList(id) {
    if (destroyListList[id] !== undefined) {
      process._rawDebug(destroyListList[id]);
      process._rawDebug();
      throw new Error(`same id added twice (${id})`);
    }
    destroyListList[id] = new Error().stack;
    _addIdToDestroyList(id);
  };

  require('async_hooks').createHook({
    init(id, ty, tr, h) {
      if (initHandles[id]) {
        throw new Error(`init called twice for same id (${id})`);
      }
      initHandles[id] = h;
    },
    before() { },
    after() { },
    destroy(id) {
      if (destroydIdsList[id] !== undefined) {
        process._rawDebug(destroydIdsList[id]);
        process._rawDebug();
        throw new Error(`destroy called for same id (${id})`);
      }
      destroydIdsList[id] = new Error().stack;
    },
  }).enable();
}

function rimrafSync(p) {
  let st;
  try {
    st = fs.lstatSync(p);
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
  exports.tmpDirName += `.${process.env.TEST_THREAD_ID}`;
}
exports.tmpDir = path.join(testRoot, exports.tmpDirName);

let opensslCli = null;
let inFreeBSDJail = null;
let localhostIPv4 = null;

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
      execSync('sysctl -n security.jail.jailed').toString() ===
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
Object.defineProperty(exports, 'opensslCli', { get: function() {
  if (opensslCli !== null) return opensslCli;

  if (process.config.variables.node_shared_openssl) {
    // use external command
    opensslCli = 'openssl';
  } else {
    // use command built from sources included in Node.js repository
    opensslCli = path.join(path.dirname(process.execPath), 'openssl-cli');
  }

  if (exports.isWindows) opensslCli += '.exe';

  const opensslCmd = spawnSync(opensslCli, ['version']);
  if (opensslCmd.status !== 0 || opensslCmd.error !== undefined) {
    // openssl command cannot be executed
    opensslCli = false;
  }
  return opensslCli;
}, enumerable: true });

Object.defineProperty(exports, 'hasCrypto', {
  get: function() {
    return Boolean(process.versions.openssl);
  }
});

Object.defineProperty(exports, 'hasFipsCrypto', {
  get: function() {
    return exports.hasCrypto && require('crypto').fips;
  }
});

{
  const pipePrefix = exports.isWindows ? '\\\\.\\pipe\\' : `${exports.tmpDir}/`;
  const pipeName = `node-test.${process.pid}.sock`;
  exports.PIPE = pipePrefix + pipeName;
}

{
  const iFaces = os.networkInterfaces();
  const re = exports.isWindows ? /Loopback Pseudo-Interface/ : /lo/;
  exports.hasIPv6 = Object.keys(iFaces).some(function(name) {
    return re.test(name) && iFaces[name].some(function(info) {
      return info.family === 'IPv6';
    });
  });
}

/*
 * Check that when running a test with
 * `$node --abort-on-uncaught-exception $file child`
 * the process aborts.
 */
exports.childShouldThrowAndAbort = function() {
  let testCmd = '';
  if (!exports.isWindows) {
    // Do not create core files, as it can take a lot of disk space on
    // continuous testing and developers' machines
    testCmd += 'ulimit -c 0 && ';
  }
  testCmd += `"${process.argv[0]}" --abort-on-uncaught-exception `;
  testCmd += `"${process.argv[1]}" child`;
  const child = exec(testCmd);
  child.on('exit', function onExit(exitCode, signal) {
    const errMsg = 'Test should have aborted ' +
                   `but instead exited with exit code ${exitCode}` +
                   ` and signal ${signal}`;
    assert(exports.nodeProcessAborted(exitCode, signal), errMsg);
  });
};

exports.ddCommand = function(filename, kilobytes) {
  if (exports.isWindows) {
    const p = path.resolve(exports.fixturesDir, 'create-file.js');
    return `"${process.argv[0]}" "${p}" "${filename}" ${kilobytes * 1024}`;
  } else {
    return `dd if=/dev/zero of="${filename}" bs=1024 count=${kilobytes}`;
  }
};


exports.spawnPwd = function(options) {
  if (exports.isWindows) {
    return spawn('cmd.exe', ['/d', '/c', 'cd'], options);
  } else {
    return spawn('pwd', [], options);
  }
};


exports.spawnSyncPwd = function(options) {
  if (exports.isWindows) {
    return spawnSync('cmd.exe', ['/d', '/c', 'cd'], options);
  } else {
    return spawnSync('pwd', [], options);
  }
};

exports.platformTimeout = function(ms) {
  if (process.config.target_defaults.default_configuration === 'Debug')
    ms = 2 * ms;

  if (global.__coverage__)
    ms = 4 * ms;

  if (exports.isAIX)
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

let knownGlobals = [
  Buffer,
  clearImmediate,
  clearInterval,
  clearTimeout,
  console,
  constructor, // Enumerable in V8 3.21.
  global,
  process,
  setImmediate,
  setInterval,
  setTimeout
];

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
  const leaked = [];

  for (const val in global) {
    if (!knownGlobals.includes(global[val])) {
      leaked.push(val);
    }
  }

  if (global.__coverage__) {
    return leaked.filter((varname) => !/^(?:cov_|__cov)/.test(varname));
  } else {
    return leaked;
  }
}
exports.leakedGlobals = leakedGlobals;

// Turn this off if the test should not check for global leaks.
exports.globalCheck = true;

process.on('exit', function() {
  if (!exports.globalCheck) return;
  const leaked = leakedGlobals();
  if (leaked.length > 0) {
    assert.fail(`Unexpected global(s) found: ${leaked.join(', ')}`);
  }
});


const mustCallChecks = [];


function runCallChecks(exitCode) {
  if (exitCode !== 0) return;

  const failed = mustCallChecks.filter(function(context) {
    if ('minimum' in context) {
      context.messageSegment = `at least ${context.minimum}`;
      return context.actual < context.minimum;
    } else {
      context.messageSegment = `exactly ${context.exact}`;
      return context.actual !== context.exact;
    }
  });

  failed.forEach(function(context) {
    console.log('Mismatched %s function calls. Expected %s, actual %d.',
                context.name,
                context.messageSegment,
                context.actual);
    console.log(context.stack.split('\n').slice(2).join('\n'));
  });

  if (failed.length) process.exit(1);
}

exports.mustCall = function(fn, exact) {
  return _mustCallInner(fn, exact, 'exact');
};

exports.mustCallAtLeast = function(fn, minimum) {
  return _mustCallInner(fn, minimum, 'minimum');
};

function _mustCallInner(fn, criteria = 1, field) {
  if (typeof fn === 'number') {
    criteria = fn;
    fn = noop;
  } else if (fn === undefined) {
    fn = noop;
  }

  if (typeof criteria !== 'number')
    throw new TypeError(`Invalid ${field} value: ${criteria}`);

  const context = {
    [field]: criteria,
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
}

exports.hasMultiLocalhost = function hasMultiLocalhost() {
  const TCP = process.binding('tcp_wrap').TCP;
  const t = new TCP();
  const ret = t.bind('127.0.0.2', exports.PORT);
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

exports.canCreateSymLink = function() {
  // On Windows, creating symlinks requires admin privileges.
  // We'll only try to run symlink test if we have enough privileges.
  // On other platforms, creating symlinks shouldn't need admin privileges
  if (exports.isWindows) {
    // whoami.exe needs to be the one from System32
    // If unix tools are in the path, they can shadow the one we want,
    // so use the full path while executing whoami
    const whoamiPath = path.join(process.env['SystemRoot'],
                                 'System32', 'whoami.exe');

    let err = false;
    let output = '';

    try {
      output = execSync(`${whoamiPath} /priv`, { timout: 1000 });
    } catch (e) {
      err = true;
    } finally {
      if (err || !output.includes('SeCreateSymbolicLinkPrivilege')) {
        return false;
      }
    }
  }

  return true;
};

exports.mustNotCall = function(msg) {
  return function mustNotCall() {
    assert.fail(msg || 'function should not have been called');
  };
};

exports.printSkipMessage = function(msg) {
  console.log(`1..0 # Skipped: ${msg}`);
};

exports.skip = function(msg) {
  exports.printSkipMessage(msg);
  process.exit(0);
};

// A stream to push an array into a REPL
function ArrayStream() {
  this.run = function(data) {
    data.forEach((line) => {
      this.emit('data', `${line}\n`);
    });
  };
}

util.inherits(ArrayStream, stream.Stream);
exports.ArrayStream = ArrayStream;
ArrayStream.prototype.readable = true;
ArrayStream.prototype.writable = true;
ArrayStream.prototype.pause = noop;
ArrayStream.prototype.resume = noop;
ArrayStream.prototype.write = noop;

// Returns true if the exit code "exitCode" and/or signal name "signal"
// represent the exit code and/or signal name of a node process that aborted,
// false otherwise.
exports.nodeProcessAborted = function nodeProcessAborted(exitCode, signal) {
  // Depending on the compiler used, node will exit with either
  // exit code 132 (SIGILL), 133 (SIGTRAP) or 134 (SIGABRT).
  let expectedExitCodes = [132, 133, 134];

  // On platforms using KSH as the default shell (like SmartOS),
  // when a process aborts, KSH exits with an exit code that is
  // greater than 256, and thus the exit code emitted with the 'exit'
  // event is null and the signal is set to either SIGILL, SIGTRAP,
  // or SIGABRT (depending on the compiler).
  const expectedSignals = ['SIGILL', 'SIGTRAP', 'SIGABRT'];

  // On Windows, 'aborts' are of 2 types, depending on the context:
  // (i) Forced access violation, if --abort-on-uncaught-exception is on
  // which corresponds to exit code 3221225477 (0xC0000005)
  // (ii) Otherwise, _exit(134) which is called in place of abort() due to
  // raising SIGABRT exiting with ambiguous exit code '3' by default
  if (exports.isWindows)
    expectedExitCodes = [0xC0000005, 134];

  // When using --abort-on-uncaught-exception, V8 will use
  // base::OS::Abort to terminate the process.
  // Depending on the compiler used, the shell or other aspects of
  // the platform used to build the node binary, this will actually
  // make V8 exit by aborting or by raising a signal. In any case,
  // one of them (exit code or signal) needs to be set to one of
  // the expected exit codes or signals.
  if (signal !== null) {
    return expectedSignals.includes(signal);
  } else {
    return expectedExitCodes.includes(exitCode);
  }
};

exports.busyLoop = function busyLoop(time) {
  const startTime = Timer.now();
  const stopTime = startTime + time;
  while (Timer.now() < stopTime) {}
};

exports.isAlive = function isAlive(pid) {
  try {
    process.kill(pid, 'SIGCONT');
    return true;
  } catch (e) {
    return false;
  }
};

function expectWarning(name, expectedMessages) {
  return exports.mustCall((warning) => {
    assert.strictEqual(warning.name, name);
    assert.ok(expectedMessages.includes(warning.message),
              `unexpected error message: "${warning.message}"`);
    // Remove a warning message after it is seen so that we guarantee that we
    // get each message only once.
    expectedMessages.splice(expectedMessages.indexOf(warning.message), 1);
  }, expectedMessages.length);
}

function expectWarningByName(name, expected) {
  if (typeof expected === 'string') {
    expected = [expected];
  }
  process.on('warning', expectWarning(name, expected));
}

function expectWarningByMap(warningMap) {
  const catchWarning = {};
  Object.keys(warningMap).forEach((name) => {
    let expected = warningMap[name];
    if (typeof expected === 'string') {
      expected = [expected];
    }
    catchWarning[name] = expectWarning(name, expected);
  });
  process.on('warning', (warning) => catchWarning[warning.name](warning));
}

// accepts a warning name and description or array of descriptions or a map
// of warning names to description(s)
// ensures a warning is generated for each name/description pair
exports.expectWarning = function(nameOrMap, expected) {
  if (typeof nameOrMap === 'string') {
    expectWarningByName(nameOrMap, expected);
  } else {
    expectWarningByMap(nameOrMap);
  }
};

Object.defineProperty(exports, 'hasIntl', {
  get: function() {
    return process.binding('config').hasIntl;
  }
});

Object.defineProperty(exports, 'hasSmallICU', {
  get: function() {
    return process.binding('config').hasSmallICU;
  }
});

// Useful for testing expected internal/error objects
exports.expectsError = function expectsError(fn, settings, exact) {
  if (typeof fn !== 'function') {
    exact = settings;
    settings = fn;
    fn = undefined;
  }
  const innerFn = exports.mustCall(function(error) {
    assert.strictEqual(error.code, settings.code);
    if ('type' in settings) {
      const type = settings.type;
      if (type !== Error && !Error.isPrototypeOf(type)) {
        throw new TypeError('`settings.type` must inherit from `Error`');
      }
      assert(error instanceof type,
             `${error.name} is not instance of ${type.name}`);
    }
    if ('message' in settings) {
      const message = settings.message;
      if (typeof message === 'string') {
        assert.strictEqual(error.message, message);
      } else {
        assert(message.test(error.message),
               `${error.message} does not match ${message}`);
      }
    }
    if ('name' in settings) {
      assert.strictEqual(error.name, settings.name);
    }
    if (error.constructor.name === 'AssertionError') {
      ['generatedMessage', 'actual', 'expected', 'operator'].forEach((key) => {
        if (key in settings) {
          const actual = error[key];
          const expected = settings[key];
          assert.strictEqual(actual, expected,
                             `${key}: expected ${expected}, not ${actual}`);
        }
      });
    }
    return true;
  }, exact);
  if (fn) {
    assert.throws(fn, innerFn);
    return;
  }
  return innerFn;
};

exports.skipIfInspectorDisabled = function skipIfInspectorDisabled() {
  if (process.config.variables.v8_enable_inspector === 0) {
    exports.skip('V8 inspector is disabled');
  }
};

exports.skipIf32Bits = function skipIf32Bits() {
  if (process.binding('config').bits < 64) {
    exports.skip('The tested feature is not available in 32bit builds');
  }
};

const arrayBufferViews = [
  Int8Array,
  Uint8Array,
  Uint8ClampedArray,
  Int16Array,
  Uint16Array,
  Int32Array,
  Uint32Array,
  Float32Array,
  Float64Array,
  DataView
];

exports.getArrayBufferViews = function getArrayBufferViews(buf) {
  const { buffer, byteOffset, byteLength } = buf;

  const out = [];
  for (const type of arrayBufferViews) {
    const { BYTES_PER_ELEMENT = 1 } = type;
    if (byteLength % BYTES_PER_ELEMENT === 0) {
      out.push(new type(buffer, byteOffset, byteLength / BYTES_PER_ELEMENT));
    }
  }
  return out;
};

// Crash the process on unhandled rejections.
exports.crashOnUnhandledRejection = function() {
  process.on('unhandledRejection',
             (err) => process.nextTick(() => { throw err; }));
};

exports.getTTYfd = function getTTYfd() {
  const tty = require('tty');
  let tty_fd = 0;
  if (!tty.isatty(tty_fd)) tty_fd++;
  else if (!tty.isatty(tty_fd)) tty_fd++;
  else if (!tty.isatty(tty_fd)) tty_fd++;
  else {
    try {
      tty_fd = fs.openSync('/dev/tty');
    } catch (e) {
      // There aren't any tty fd's available to use.
      return -1;
    }
  }
  return tty_fd;
};

// Hijack stdout and stderr
const stdWrite = {};
function hijackStdWritable(name, listener) {
  const stream = process[name];
  const _write = stdWrite[name] = stream.write;

  stream.writeTimes = 0;
  stream.write = function(data, callback) {
    try {
      listener(data);
    } catch (e) {
      process.nextTick(() => { throw e; });
    }

    _write.call(stream, data, callback);
    stream.writeTimes++;
  };
}

function restoreWritable(name) {
  process[name].write = stdWrite[name];
  delete process[name].writeTimes;
}

function onResolvedOrRejected(promise, callback) {
  return promise.then((result) => {
    callback();
    return result;
  }, (error) => {
    callback();
    throw error;
  });
}

function timeoutPromise(error, timeoutMs) {
  let clearCallback = null;
  let done = false;
  const promise = onResolvedOrRejected(new Promise((resolve, reject) => {
    const timeout = setTimeout(() => reject(error), timeoutMs);
    clearCallback = () => {
      if (done)
        return;
      clearTimeout(timeout);
      resolve();
    };
  }), () => done = true);
  promise.clear = clearCallback;
  return promise;
}

exports.hijackStdout = hijackStdWritable.bind(null, 'stdout');
exports.hijackStderr = hijackStdWritable.bind(null, 'stderr');
exports.restoreStdout = restoreWritable.bind(null, 'stdout');
exports.restoreStderr = restoreWritable.bind(null, 'stderr');

let fd = 2;
exports.firstInvalidFD = function firstInvalidFD() {
  // Get first known bad file descriptor.
  try {
    while (fs.fstatSync(++fd));
  } catch (e) {}
  return fd;
};

exports.fires = function fires(promise, error, timeoutMs) {
  if (!timeoutMs && util.isNumber(error)) {
    timeoutMs = error;
    error = null;
  }
  if (!error)
    error = 'timeout';
  if (!timeoutMs)
    timeoutMs = 100;
  const timeout = timeoutPromise(error, timeoutMs);
  return Promise.race([
    onResolvedOrRejected(promise, () => timeout.clear()),
    timeout
  ]);
};
