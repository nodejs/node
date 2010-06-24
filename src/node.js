(function (process) {

process.global.process = process;
process.global.global = process.global;
global.GLOBAL = global;

/** deprecation errors ************************************************/

function removed (reason) {
  return function () {
    throw new Error(reason)
  }
}

GLOBAL.__module = removed("'__module' has been renamed to 'module'");
GLOBAL.include = removed("include(module) has been removed. Use require(module)");
GLOBAL.puts = removed("puts() has moved. Use require('sys') to bring it back.");
GLOBAL.print = removed("print() has moved. Use require('sys') to bring it back.");
GLOBAL.p = removed("p() has moved. Use require('sys') to bring it back.");
process.debug = removed("process.debug() has moved. Use require('sys') to bring it back.");
process.error = removed("process.error() has moved. Use require('sys') to bring it back.");
process.watchFile = removed("process.watchFile() has moved to fs.watchFile()");
process.unwatchFile = removed("process.unwatchFile() has moved to fs.unwatchFile()");
process.mixin = removed('process.mixin() has been removed.');
process.createChildProcess = removed("childProcess API has changed. See doc/api.txt.");
process.inherits = removed("process.inherits() has moved to sys.inherits.");

process.assert = function (x, msg) {
  if (!x) throw new Error(msg || "assertion error");
};

var evalcxMsg;
process.evalcx = function () {
  if (!evalcxMsg) {
    process.binding('stdio').writeError(evalcxMsg =
      "process.evalcx is deprecated. Use Script.runInNewContext instead.\n");
  }
  return process.binding('evals').Script
    .runInNewContext.apply(null, arguments);
};

// nextTick()

var nextTickQueue = [];

process._tickCallback = function () {
  for (var l = nextTickQueue.length; l; l--) {
    nextTickQueue.shift()();
  }
};

process.nextTick = function (callback) {
  nextTickQueue.push(callback);
  process._needTickCallback();
};

// Module System
var module = {}
process.compile("(function (exports) {"
               + process.binding("natives").module
               + "\n})", "module")(module);

// TODO: make sure that event module gets loaded here once it's
// factored out of module.js
// module.require("events");

// Signal Handlers
(function() {
  var signalWatchers = {};
    addListener = process.addListener,
    removeListener = process.removeListener;

  function isSignal (event) {
    return event.slice(0, 3) === 'SIG' && process.hasOwnProperty(event);
  };

  // Wrap addListener for the special signal types
  process.addListener = function (type, listener) {
    var ret = addListener.apply(this, arguments);
    if (isSignal(type)) {
      if (!signalWatchers.hasOwnProperty(type)) {
        var b = process.binding('signal_watcher'),
          w = new b.SignalWatcher(process[type]);
          w.callback = function () {
            process.emit(type);
          }
        signalWatchers[type] = w;
        w.start();
      } else if (this.listeners(type).length === 1) {
        signalWatchers[event].start();
      }
    }

    return ret;
  }

  process.removeListener = function (type, listener) {
    var ret = removeListener.apply(this, arguments);
    if (isSignal(type)) {
      process.assert(signalWatchers.hasOwnProperty(type));

      if (this.listeners(type).length === 0) {
        signalWatchers[type].stop();
      }
    }

    return ret;
  }
})();

// Timers
function addTimerListener (callback) {
  var timer = this;
  // Special case the no param case to avoid the extra object creation.
  if (arguments.length > 2) {
    var args = Array.prototype.slice.call(arguments, 2);
    timer.callback = function () { callback.apply(timer, args); };
  } else {
    timer.callback = callback;
  }
}

global.setTimeout = function (callback, after) {
  var timer = new process.Timer();
  addTimerListener.apply(timer, arguments);
  timer.start(after, 0);
  return timer;
};

global.setInterval = function (callback, repeat) {
  var timer = new process.Timer();
  addTimerListener.apply(timer, arguments);
  timer.start(repeat, repeat);
  return timer;
};

global.clearTimeout = function (timer) {
  if (timer instanceof process.Timer) {
    timer.stop();
  }
};

global.clearInterval = global.clearTimeout;

var stdout;
process.__defineGetter__('stdout', function () {
  if (stdout) return stdout;

  var binding = process.binding('stdio'),
      net = module.requireNative('net'),
      fs = module.requireNative('fs'),
      fd = binding.stdoutFD;

  if (binding.isStdoutBlocking()) {
    stdout = new fs.WriteStream(null, {fd: fd});
  } else {
    stdout = new net.Stream(fd);
    // FIXME Should probably have an option in net.Stream to create a stream from
    // an existing fd which is writable only. But for now we'll just add
    // this hack and set the `readable` member to false.
    // Test: ./node test/fixtures/echo.js < /etc/passwd
    stdout.readable = false;
  }

  return stdout;
});

var stdin;
process.openStdin = function () {
  if (stdin) return stdin;

  var binding = process.binding('stdio'),
      net = module.requireNative('net'),
      fs = module.requireNative('fs'),
      fd = binding.openStdin();

  if (binding.isStdinBlocking()) {
    stdin = new fs.ReadStream(null, {fd: fd});
  } else {
    stdin = new net.Stream(fd);
    stdin.readable = true;
  }

  stdin.resume();

  return stdin;
};


global.console = {};

global.console.log = function (x) {
  process.stdout.write(x + '\n');
};


process.exit = function (code) {
  process.emit("exit");
  process.reallyExit(code);
};

var cwd = process.cwd();
var path = module.requireNative('path');

// Make process.argv[0] and process.argv[1] into full paths.
if (process.argv[0].indexOf('/') > 0) {
  process.argv[0] = path.join(cwd, process.argv[0]);
}

if (process.argv[1]) {
  if (process.argv[1].charAt(0) != "/" && !(/^http:\/\//).exec(process.argv[1])) {
    process.argv[1] = path.join(cwd, process.argv[1]);
  }

  module.runMain();
} else {
  // No arguments, run the repl
  var repl = module.requireNative('repl');
  console.log("Type '.help' for options.");
  repl.start();
}

// All our arguments are loaded. We've evaluated all of the scripts. We
// might even have created TCP servers. Now we enter the main eventloop. If
// there are no watchers on the loop (except for the ones that were
// ev_unref'd) then this function exits. As long as there are active
// watchers, it blocks.
process.loop();

process.emit("exit");

});
