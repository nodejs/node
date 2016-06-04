'use strict';

const internalUtil = require('internal/util');
const util = require('util');
const path = require('path');
const net = require('net');
const vm = require('vm');
const Module = require('module');
const repl = require('repl');
const inherits = util.inherits;
const assert = require('assert');
const spawn = require('child_process').spawn;
const Buffer = require('buffer').Buffer;

exports.start = function(argv, stdin, stdout) {
  argv || (argv = process.argv.slice(2));

  if (argv.length < 1) {
    console.error('Usage: node debug script.js');
    console.error('       node debug <host>:<port>');
    console.error('       node debug -p <pid>');
    process.exit(1);
  }

  // Setup input/output streams
  stdin = stdin || process.stdin;
  stdout = stdout || process.stdout;

  const args = [`--debug-brk=${exports.port}`].concat(argv);
  const interface_ = new Interface(stdin, stdout, args);

  stdin.resume();

  process.on('uncaughtException', function(e) {
    internalUtil.error('There was an internal error in Node\'s debugger. ' +
                       'Please report this bug.');
    console.error(e.message);
    console.error(e.stack);
    if (interface_.child) interface_.child.kill();
    process.exit(1);
  });
};

exports.port = process.debugPort;


//
// Parser/Serializer for V8 debugger protocol
// https://github.com/v8/v8/wiki/Debugging-Protocol
//
// Usage:
//    p = new Protocol();
//
//    p.onResponse = function(res) {
//      // do stuff with response from V8
//    };
//
//    socket.setEncoding('utf8');
//    socket.on('data', function(s) {
//      // Pass strings into the protocol
//      p.execute(s);
//    });
//
//
function Protocol() {
  this._newRes();
}
exports.Protocol = Protocol;


Protocol.prototype._newRes = function(raw) {
  this.res = { raw: raw || '', headers: {} };
  this.state = 'headers';
  this.reqSeq = 1;
  this.execute('');
};


Protocol.prototype.execute = function(d) {
  var res = this.res;
  res.raw += d;

  switch (this.state) {
    case 'headers':
      var endHeaderIndex = res.raw.indexOf('\r\n\r\n');

      if (endHeaderIndex < 0) break;

      var rawHeader = res.raw.slice(0, endHeaderIndex);
      var endHeaderByteIndex = Buffer.byteLength(rawHeader, 'utf8');
      var lines = rawHeader.split('\r\n');
      for (var i = 0; i < lines.length; i++) {
        var kv = lines[i].split(/: +/);
        res.headers[kv[0]] = kv[1];
      }

      this.contentLength = +res.headers['Content-Length'];
      this.bodyStartByteIndex = endHeaderByteIndex + 4;

      this.state = 'body';

      var len = Buffer.byteLength(res.raw, 'utf8');
      if (len - this.bodyStartByteIndex < this.contentLength) {
        break;
      }
      // falls through
    case 'body':
      var resRawByteLength = Buffer.byteLength(res.raw, 'utf8');

      if (resRawByteLength - this.bodyStartByteIndex >= this.contentLength) {
        var buf = Buffer.allocUnsafe(resRawByteLength);
        buf.write(res.raw, 0, resRawByteLength, 'utf8');
        res.body =
            buf.slice(this.bodyStartByteIndex,
                      this.bodyStartByteIndex +
                      this.contentLength).toString('utf8');
        // JSON parse body?
        res.body = res.body.length ? JSON.parse(res.body) : {};

        // Done!
        this.onResponse(res);

        this._newRes(buf.slice(this.bodyStartByteIndex +
                               this.contentLength).toString('utf8'));
      }
      break;

    default:
      throw new Error('Unknown state');
  }
};


Protocol.prototype.serialize = function(req) {
  req.type = 'request';
  req.seq = this.reqSeq++;
  var json = JSON.stringify(req);
  return 'Content-Length: ' + Buffer.byteLength(json, 'utf8') +
         '\r\n\r\n' + json;
};


const NO_FRAME = -1;

function Client() {
  net.Socket.call(this);
  var protocol = this.protocol = new Protocol(this);
  this._reqCallbacks = [];
  var socket = this;

  this.currentFrame = NO_FRAME;
  this.currentSourceLine = -1;
  this.handles = {};
  this.scripts = {};
  this.breakpoints = [];

  // Note that 'Protocol' requires strings instead of Buffers.
  socket.setEncoding('utf8');
  socket.on('data', function(d) {
    protocol.execute(d);
  });

  protocol.onResponse = (res) => this._onResponse(res);
}
inherits(Client, net.Socket);
exports.Client = Client;


Client.prototype._addHandle = function(desc) {
  if (desc === null || typeof desc !== 'object' ||
      typeof desc.handle !== 'number') {
    return;
  }

  this.handles[desc.handle] = desc;

  if (desc.type === 'script') {
    this._addScript(desc);
  }
};


const natives = process.binding('natives');


Client.prototype._addScript = function(desc) {
  this.scripts[desc.id] = desc;
  if (desc.name) {
    desc.isNative = (desc.name.replace('.js', '') in natives) ||
                    desc.name == 'node.js';
  }
};


Client.prototype._removeScript = function(desc) {
  this.scripts[desc.id] = undefined;
};


Client.prototype._onResponse = function(res) {
  var cb;
  var index = -1;

  this._reqCallbacks.some(function(fn, i) {
    if (fn.request_seq == res.body.request_seq) {
      cb = fn;
      index = i;
      return true;
    }
  });

  var self = this;
  var handled = false;

  if (res.headers.Type == 'connect') {
    // Request a list of scripts for our own storage.
    self.reqScripts();
    self.emit('ready');
    handled = true;

  } else if (res.body && res.body.event == 'break') {
    this.emit('break', res.body);
    handled = true;

  } else if (res.body && res.body.event == 'exception') {
    this.emit('exception', res.body);
    handled = true;

  } else if (res.body && res.body.event == 'afterCompile') {
    this._addHandle(res.body.body.script);
    handled = true;

  } else if (res.body && res.body.event == 'scriptCollected') {
    // ???
    this._removeScript(res.body.body.script);
    handled = true;

  } else if (res.body && res.body.event === 'compileError') {
    // This event is not used anywhere right now, perhaps somewhere in the
    // future?
    handled = true;
  }

  if (cb) {
    this._reqCallbacks.splice(index, 1);
    handled = true;

    var err = res.success === false && (res.message || true) ||
              res.body.success === false && (res.body.message || true);
    cb(err, res.body && res.body.body || res.body, res);
  }

  if (!handled) this.emit('unhandledResponse', res.body);
};


Client.prototype.req = function(req, cb) {
  this.write(this.protocol.serialize(req));
  cb.request_seq = req.seq;
  this._reqCallbacks.push(cb);
};


Client.prototype.reqVersion = function(cb) {
  cb = cb || function() {};
  this.req({ command: 'version' }, function(err, body, res) {
    if (err) return cb(err);
    cb(null, res.body.body.V8Version, res.body.running);
  });
};


Client.prototype.reqLookup = function(refs, cb) {
  var self = this;

  // TODO: We have a cache of handle's we've already seen in this.handles
  // This can be used if we're careful.
  var req = {
    command: 'lookup',
    arguments: {
      handles: refs
    }
  };

  cb = cb || function() {};
  this.req(req, function(err, res) {
    if (err) return cb(err);
    for (var ref in res) {
      if (res[ref] !== null && typeof res[ref] === 'object') {
        self._addHandle(res[ref]);
      }
    }

    cb(null, res);
  });
};

Client.prototype.reqScopes = function(cb) {
  const self = this;
  const req = {
    command: 'scopes',
    arguments: {}
  };

  cb = cb || function() {};
  this.req(req, function(err, res) {
    if (err) return cb(err);
    var refs = res.scopes.map(function(scope) {
      return scope.object.ref;
    });

    self.reqLookup(refs, function(err, res) {
      if (err) return cb(err);

      var globals = Object.keys(res).map(function(key) {
        return res[key].properties.map(function(prop) {
          return prop.name;
        });
      });

      cb(null, globals.reverse());
    });
  });
};

// This is like reqEval, except it will look up the expression in each of the
// scopes associated with the current frame.
Client.prototype.reqEval = function(expression, cb) {
  var self = this;

  if (this.currentFrame == NO_FRAME) {
    // Only need to eval in global scope.
    this.reqFrameEval(expression, NO_FRAME, cb);
    return;
  }

  cb = cb || function() {};
  // Otherwise we need to get the current frame to see which scopes it has.
  this.reqBacktrace(function(err, bt) {
    if (err || !bt.frames) {
      // ??
      return cb(null, {});
    }

    var frame = bt.frames[self.currentFrame];

    var evalFrames = frame.scopes.map(function(s) {
      if (!s) return;
      var x = bt.frames[s.index];
      if (!x) return;
      return x.index;
    });

    self._reqFramesEval(expression, evalFrames, cb);
  });
};


// Finds the first scope in the array in which the expression evals.
Client.prototype._reqFramesEval = function(expression, evalFrames, cb) {
  if (evalFrames.length == 0) {
    // Just eval in global scope.
    this.reqFrameEval(expression, NO_FRAME, cb);
    return;
  }

  var self = this;
  var i = evalFrames.shift();

  cb = cb || function() {};
  this.reqFrameEval(expression, i, function(err, res) {
    if (!err) return cb(null, res);
    self._reqFramesEval(expression, evalFrames, cb);
  });
};


Client.prototype.reqFrameEval = function(expression, frame, cb) {
  var self = this;
  var req = {
    command: 'evaluate',
    arguments: { expression: expression }
  };

  if (frame == NO_FRAME) {
    req.arguments.global = true;
  } else {
    req.arguments.frame = frame;
  }

  cb = cb || function() {};
  this.req(req, function(err, res) {
    if (!err) self._addHandle(res);
    cb(err, res);
  });
};


// reqBacktrace(cb)
// TODO: from, to, bottom
Client.prototype.reqBacktrace = function(cb) {
  this.req({ command: 'backtrace', arguments: { inlineRefs: true } }, cb);
};


// reqSetExceptionBreak(type, cb)
// TODO: from, to, bottom
Client.prototype.reqSetExceptionBreak = function(type, cb) {
  this.req({
    command: 'setexceptionbreak',
    arguments: { type: type, enabled: true }
  }, cb);
};


// Returns an array of objects like this:
//
//   { handle: 11,
//     type: 'script',
//     name: 'node.js',
//     id: 14,
//     lineOffset: 0,
//     columnOffset: 0,
//     lineCount: 562,
//     sourceStart: '(function(process) {\n\n  ',
//     sourceLength: 15939,
//     scriptType: 2,
//     compilationType: 0,
//     context: { ref: 10 },
//     text: 'node.js (lines: 562)' }
//
Client.prototype.reqScripts = function(cb) {
  var self = this;
  cb = cb || function() {};

  this.req({ command: 'scripts' }, function(err, res) {
    if (err) return cb(err);

    for (var i = 0; i < res.length; i++) {
      self._addHandle(res[i]);
    }
    cb(null);
  });
};


Client.prototype.reqContinue = function(cb) {
  this.currentFrame = NO_FRAME;
  this.req({ command: 'continue' }, cb);
};

Client.prototype.listbreakpoints = function(cb) {
  this.req({ command: 'listbreakpoints' }, cb);
};

Client.prototype.setBreakpoint = function(req, cb) {
  req = {
    command: 'setbreakpoint',
    arguments: req
  };

  this.req(req, cb);
};

Client.prototype.clearBreakpoint = function(req, cb) {
  req = {
    command: 'clearbreakpoint',
    arguments: req
  };

  this.req(req, cb);
};

Client.prototype.reqSource = function(from, to, cb) {
  var req = {
    command: 'source',
    fromLine: from,
    toLine: to
  };

  this.req(req, cb);
};


// client.next(1, cb);
Client.prototype.step = function(action, count, cb) {
  var req = {
    command: 'continue',
    arguments: { stepaction: action, stepcount: count }
  };

  this.currentFrame = NO_FRAME;
  this.req(req, cb);
};


Client.prototype.mirrorObject = function(handle, depth, cb) {
  var self = this;

  var val;

  if (handle.type === 'object') {
    // The handle looks something like this:
    // { handle: 8,
    //   type: 'object',
    //   className: 'Object',
    //   constructorFunction: { ref: 9 },
    //   protoObject: { ref: 4 },
    //   prototypeObject: { ref: 2 },
    //   properties: [ { name: 'hello', propertyType: 1, ref: 10 } ],
    //   text: '#<an Object>' }

    // For now ignore the className and constructor and prototype.
    // TJ's method of object inspection would probably be good for this:
    // https://groups.google.com/forum/?pli=1#!topic/nodejs-dev/4gkWBOimiOg

    var propertyRefs = handle.properties.map(function(p) {
      return p.ref;
    });

    cb = cb || function() {};
    this.reqLookup(propertyRefs, function(err, res) {
      if (err) {
        internalUtil.error('problem with reqLookup');
        cb(null, handle);
        return;
      }

      var mirror;
      var waiting = 1;

      if (handle.className == 'Array') {
        mirror = [];
      } else if (handle.className == 'Date') {
        mirror = new Date(handle.value);
      } else {
        mirror = {};
      }


      var keyValues = [];
      handle.properties.forEach(function(prop, i) {
        var value = res[prop.ref];
        var mirrorValue;
        if (value) {
          mirrorValue = value.value ? value.value : value.text;
        } else {
          mirrorValue = '[?]';
        }

        // Skip the 'length' property.
        if (Array.isArray(mirror) && prop.name === 'length') {
          return;
        }

        keyValues[i] = {
          name: prop.name,
          value: mirrorValue
        };
        if (value && value.handle && depth > 0) {
          waiting++;
          self.mirrorObject(value, depth - 1, function(err, result) {
            if (!err) keyValues[i].value = result;
            waitForOthers();
          });
        }
      });

      waitForOthers();
      function waitForOthers() {
        if (--waiting === 0) {
          keyValues.forEach(function(kv) {
            mirror[kv.name] = kv.value;
          });
          cb(null, mirror);
        }
      }
    });
    return;
  } else if (handle.type === 'function') {
    val = function() {};
  } else if (handle.type === 'null') {
    val = null;
  } else if (handle.value !== undefined) {
    val = handle.value;
  } else if (handle.type === 'undefined') {
    val = undefined;
  } else {
    val = handle;
  }
  process.nextTick(cb, null, val);
};


Client.prototype.fullTrace = function(cb) {
  var self = this;

  cb = cb || function() {};
  this.reqBacktrace(function(err, trace) {
    if (err) return cb(err);
    if (trace.totalFrames <= 0) return cb(Error('No frames'));

    var refs = [];

    for (var i = 0; i < trace.frames.length; i++) {
      var frame = trace.frames[i];
      // looks like this:
      // { type: 'frame',
      //   index: 0,
      //   receiver: { ref: 1 },
      //   func: { ref: 0 },
      //   script: { ref: 7 },
      //   constructCall: false,
      //   atReturn: false,
      //   debuggerFrame: false,
      //   arguments: [],
      //   locals: [],
      //   position: 160,
      //   line: 7,
      //   column: 2,
      //   sourceLineText: '  debugger;',
      //   scopes: [ { type: 1, index: 0 }, { type: 0, index: 1 } ],
      //   text: '#00 blah() /home/ryan/projects/node/test-debug.js l...' }
      refs.push(frame.script.ref);
      refs.push(frame.func.ref);
      refs.push(frame.receiver.ref);
    }

    self.reqLookup(refs, function(err, res) {
      if (err) return cb(err);

      for (var i = 0; i < trace.frames.length; i++) {
        var frame = trace.frames[i];
        frame.script = res[frame.script.ref];
        frame.func = res[frame.func.ref];
        frame.receiver = res[frame.receiver.ref];
      }

      cb(null, trace);
    });
  });
};


const commands = [
  [
    'run (r)',
    'cont (c)',
    'next (n)',
    'step (s)',
    'out (o)',
    'backtrace (bt)',
    'setBreakpoint (sb)',
    'clearBreakpoint (cb)'
  ],
  [
    'watch',
    'unwatch',
    'watchers',
    'repl',
    'exec',
    'restart',
    'kill',
    'list',
    'scripts',
    'breakOnException',
    'breakpoints',
    'version'
  ]
];


var helpMessage = 'Commands: ' + commands.map(function(group) {
  return group.join(', ');
}).join(',\n');

// Previous command received. Initialize to empty command.
var lastCommand = '\n';


function SourceUnderline(sourceText, position, repl) {
  if (!sourceText) return '';

  const head = sourceText.slice(0, position);
  var tail = sourceText.slice(position);

  // Colourize char if stdout supports colours
  if (repl.useColors) {
    tail = tail.replace(/(.+?)([^\w]|$)/, '\u001b[32m$1\u001b[39m$2');
  }

  // Return source line with coloured char at `position`
  return [
    head,
    tail
  ].join('');
}


function SourceInfo(body) {
  var result = body.exception ? 'exception in ' : 'break in ';

  if (body.script) {
    if (body.script.name) {
      var name = body.script.name;
      const dir = path.resolve() + '/';

      // Change path to relative, if possible
      if (name.indexOf(dir) === 0) {
        name = name.slice(dir.length);
      }

      result += name;
    } else {
      result += '[unnamed]';
    }
  }
  result += ':';
  result += body.sourceLine + 1;

  if (body.exception) result += '\n' + body.exception.text;

  return result;
}

// This class is the repl-enabled debugger interface which is invoked on
// "node debug"
function Interface(stdin, stdout, args) {
  var self = this;

  this.stdin = stdin;
  this.stdout = stdout;
  this.args = args;

  // Two eval modes are available: controlEval and debugEval
  // But controlEval is used by default
  var opts = {
    prompt: 'debug> ',
    input: this.stdin,
    output: this.stdout,
    eval: (code, ctx, file, cb) => this.controlEval(code, ctx, file, cb),
    useGlobal: false,
    ignoreUndefined: true
  };
  if (parseInt(process.env['NODE_NO_READLINE'], 10)) {
    opts.terminal = false;
  } else if (parseInt(process.env['NODE_FORCE_READLINE'], 10)) {
    opts.terminal = true;

    // Emulate Ctrl+C if we're emulating terminal
    if (!this.stdout.isTTY) {
      process.on('SIGINT', function() {
        self.repl.rli.emit('SIGINT');
      });
    }
  }
  if (parseInt(process.env['NODE_DISABLE_COLORS'], 10)) {
    opts.useColors = false;
  }

  this.repl = repl.start(opts);

  // Do not print useless warning
  repl._builtinLibs.splice(repl._builtinLibs.indexOf('repl'), 1);

  // Kill child process when main process dies
  this.repl.on('exit', function() {
    process.exit(0);
  });

  // Handle all possible exits
  process.on('exit', () => this.killChild());
  process.once('SIGTERM', process.exit.bind(process, 0));
  process.once('SIGHUP', process.exit.bind(process, 0));

  var proto = Interface.prototype;
  const ignored = ['pause', 'resume', 'exitRepl', 'handleBreak',
                   'requireConnection', 'killChild', 'trySpawn',
                   'controlEval', 'debugEval', 'print', 'childPrint',
                   'clearline'];
  const shortcut = {
    'run': 'r',
    'cont': 'c',
    'next': 'n',
    'step': 's',
    'out': 'o',
    'backtrace': 'bt',
    'setBreakpoint': 'sb',
    'clearBreakpoint': 'cb',
    'pause_': 'pause'
  };

  function defineProperty(key, protoKey) {
    // Check arity
    var fn = proto[protoKey].bind(self);

    if (proto[protoKey].length === 0) {
      Object.defineProperty(self.repl.context, key, {
        get: fn,
        enumerable: true,
        configurable: false
      });
    } else {
      self.repl.context[key] = fn;
    }
  }

  // Copy all prototype methods in repl context
  // Setup them as getters if possible
  for (var i in proto) {
    if (Object.prototype.hasOwnProperty.call(proto, i) &&
        ignored.indexOf(i) === -1) {
      defineProperty(i, i);
      if (shortcut[i]) defineProperty(shortcut[i], i);
    }
  }

  this.killed = false;
  this.waiting = null;
  this.paused = 0;
  this.context = this.repl.context;
  this.history = {
    debug: [],
    control: []
  };
  this.breakpoints = [];
  this._watchers = [];

  // Run script automatically
  this.pause();

  setImmediate(() => { this.run(); });
}


// Stream control


Interface.prototype.pause = function() {
  if (this.killed || this.paused++ > 0) return this;
  this.repl.rli.pause();
  this.stdin.pause();
  return this;
};

Interface.prototype.resume = function(silent) {
  if (this.killed || this.paused === 0 || --this.paused !== 0) return this;
  this.repl.rli.resume();
  if (silent !== true) {
    this.repl.displayPrompt();
  }
  this.stdin.resume();

  if (this.waiting) {
    this.waiting();
    this.waiting = null;
  }
  return this;
};


// Clear current line
Interface.prototype.clearline = function() {
  if (this.stdout.isTTY) {
    this.stdout.cursorTo(0);
    this.stdout.clearLine(1);
  } else {
    this.stdout.write('\b');
  }
};

// Print text to output stream
Interface.prototype.print = function(text, oneline) {
  if (this.killed) return;
  this.clearline();

  this.stdout.write(typeof text === 'string' ? text : util.inspect(text));

  if (oneline !== true) {
    this.stdout.write('\n');
  }
};

// Format and print text from child process
Interface.prototype.childPrint = function(text) {
  this.print(text.toString().split(/\r\n|\r|\n/g).filter(function(chunk) {
    return chunk;
  }).map(function(chunk) {
    return '< ' + chunk;
  }).join('\n'));
  this.repl.displayPrompt(true);
};

// Errors formatting
Interface.prototype.error = function(text) {
  this.print(text);
  this.resume();
};


// Debugger's `break` event handler
Interface.prototype.handleBreak = function(r) {
  var self = this;

  this.pause();

  // Save execution context's data
  this.client.currentSourceLine = r.sourceLine;
  this.client.currentSourceLineText = r.sourceLineText;
  this.client.currentSourceColumn = r.sourceColumn;
  this.client.currentFrame = 0;
  this.client.currentScript = r.script && r.script.name;

  // Print break data
  this.print(SourceInfo(r));

  // Show watchers' values
  this.watchers(true, function(err) {
    if (err) return self.error(err);

    // And list source
    self.list(2);

    self.resume(true);
  });
};


// Internal method for checking connection state
Interface.prototype.requireConnection = function() {
  if (!this.client) {
    this.error('App isn\'t running... Try `run` instead');
    return false;
  }
  return true;
};


// Evals

// Used for debugger's commands evaluation and execution
Interface.prototype.controlEval = function(code, context, filename, callback) {
  try {
    // Repeat last command if empty line are going to be evaluated
    if (code === '\n') {
      code = lastCommand;
    } else {
      lastCommand = code;
    }

    // exec process.title => exec("process.title");
    var match = code.match(/^\s*exec\s+([^\n]*)/);
    if (match) {
      code = 'exec(' + JSON.stringify(match[1]) + ')';
    }

    var result = vm.runInContext(code, context, filename);

    // Repl should not ask for next command
    // if current one was asynchronous.
    if (this.paused === 0) return callback(null, result);

    // Add a callback for asynchronous command
    // (it will be automatically invoked by .resume() method
    this.waiting = function() {
      callback(null, result);
    };
  } catch (e) {
    callback(e);
  }
};

// Used for debugger's remote evaluation (`repl`) commands
Interface.prototype.debugEval = function(code, context, filename, callback) {
  if (!this.requireConnection()) return;

  const self = this;
  const client = this.client;

  // Repl asked for scope variables
  if (code === '.scope') {
    client.reqScopes(callback);
    return;
  }

  var frame = client.currentFrame === NO_FRAME ? frame : undefined;

  self.pause();

  // Request remote evaluation globally or in current frame
  client.reqFrameEval(code, frame, function(err, res) {
    if (err) {
      callback(err);
      self.resume(true);
      return;
    }

    // Request object by handles (and it's sub-properties)
    client.mirrorObject(res, 3, function(err, mirror) {
      callback(null, mirror);
      self.resume(true);
    });
  });
};


// Utils

// Adds spaces and prefix to number
// maxN is a maximum number we should have space for
function leftPad(n, prefix, maxN) {
  const s = n.toString();
  const nchars = Math.max(2, String(maxN).length) + 1;
  const nspaces = nchars - s.length - 1;

  return prefix + ' '.repeat(nspaces) + s;
}


// Commands


// Print help message
Interface.prototype.help = function() {
  this.print(helpMessage);
};


// Run script
Interface.prototype.run = function() {
  var callback = arguments[0];

  if (this.child) {
    this.error('App is already running... Try `restart` instead');
    callback && callback(true);
  } else {
    this.trySpawn(callback);
  }
};


// Restart script
Interface.prototype.restart = function() {
  if (!this.requireConnection()) return;

  var self = this;

  self.pause();
  self.killChild();

  // XXX need to wait a little bit for the restart to work?
  setTimeout(function() {
    self.trySpawn();
    self.resume();
  }, 1000);
};


// Print version
Interface.prototype.version = function() {
  if (!this.requireConnection()) return;

  var self = this;

  this.pause();
  this.client.reqVersion(function(err, v) {
    if (err) {
      self.error(err);
    } else {
      self.print(v);
    }
    self.resume();
  });
};

// List source code
Interface.prototype.list = function(delta) {
  if (!this.requireConnection()) return;

  delta || (delta = 5);

  const self = this;
  const client = this.client;
  const from = client.currentSourceLine - delta + 1;
  const to = client.currentSourceLine + delta + 1;

  self.pause();
  client.reqSource(from, to, function(err, res) {
    if (err || !res) {
      self.error('You can\'t list source code right now');
      self.resume();
      return;
    }

    var lines = res.source.split('\n');
    for (var i = 0; i < lines.length; i++) {
      var lineno = res.fromLine + i + 1;
      if (lineno < from || lineno > to) continue;

      const current = lineno == 1 + client.currentSourceLine;
      const breakpoint = client.breakpoints.some(function(bp) {
        return (bp.scriptReq === client.currentScript ||
                bp.script === client.currentScript) &&
                bp.line == lineno;
      });

      if (lineno == 1) {
        // The first line needs to have the module wrapper filtered out of
        // it.
        var wrapper = Module.wrapper[0];
        lines[i] = lines[i].slice(wrapper.length);

        client.currentSourceColumn -= wrapper.length;
      }

      // Highlight executing statement
      var line;
      if (current) {
        line = SourceUnderline(lines[i],
                               client.currentSourceColumn,
                               self.repl);
      } else {
        line = lines[i];
      }

      var prefixChar = ' ';
      if (current) {
        prefixChar = '>';
      } else if (breakpoint) {
        prefixChar = '*';
      }

      self.print(leftPad(lineno, prefixChar, to) + ' ' + line);
    }
    self.resume();
  });
};

// Print backtrace
Interface.prototype.backtrace = function() {
  if (!this.requireConnection()) return;

  const self = this;
  const client = this.client;

  self.pause();
  client.fullTrace(function(err, bt) {
    if (err) {
      self.error('Can\'t request backtrace now');
      self.resume();
      return;
    }

    if (bt.totalFrames == 0) {
      self.print('(empty stack)');
    } else {
      const trace = [];
      const firstFrameNative = bt.frames[0].script.isNative;

      for (var i = 0; i < bt.frames.length; i++) {
        var frame = bt.frames[i];
        if (!firstFrameNative && frame.script.isNative) break;

        var text = '#' + i + ' ';
        if (frame.func.inferredName && frame.func.inferredName.length > 0) {
          text += frame.func.inferredName + ' ';
        }
        text += path.basename(frame.script.name) + ':';
        text += (frame.line + 1) + ':' + (frame.column + 1);

        trace.push(text);
      }

      self.print(trace.join('\n'));
    }

    self.resume();
  });
};


// First argument tells if it should display internal node scripts or not
// (available only for internal debugger's functions)
Interface.prototype.scripts = function() {
  if (!this.requireConnection()) return;

  const client = this.client;
  const displayNatives = arguments[0] || false;
  const scripts = [];

  this.pause();
  for (var id in client.scripts) {
    var script = client.scripts[id];
    if (script !== null && typeof script === 'object' && script.name) {
      if (displayNatives ||
          script.name == client.currentScript ||
          !script.isNative) {
        scripts.push(
            (script.name == client.currentScript ? '* ' : '  ') +
            id + ': ' +
            path.basename(script.name)
        );
      }
    }
  }
  this.print(scripts.join('\n'));
  this.resume();
};


// Continue execution of script
Interface.prototype.cont = function() {
  if (!this.requireConnection()) return;
  this.pause();

  var self = this;
  this.client.reqContinue(function(err) {
    if (err) self.error(err);
    self.resume();
  });
};


// Step commands generator
Interface.stepGenerator = function(type, count) {
  return function() {
    if (!this.requireConnection()) return;

    var self = this;

    self.pause();
    self.client.step(type, count, function(err, res) {
      if (err) self.error(err);
      self.resume();
    });
  };
};


// Jump to next command
Interface.prototype.next = Interface.stepGenerator('next', 1);


// Step in
Interface.prototype.step = Interface.stepGenerator('in', 1);


// Step out
Interface.prototype.out = Interface.stepGenerator('out', 1);


// Watch
Interface.prototype.watch = function(expr) {
  this._watchers.push(expr);
};

// Unwatch
Interface.prototype.unwatch = function(expr) {
  var index = this._watchers.indexOf(expr);

  // Unwatch by expression
  // or
  // Unwatch by watcher number
  this._watchers.splice(index !== -1 ? index : +expr, 1);
};

// List watchers
Interface.prototype.watchers = function() {
  var self = this;
  var verbose = arguments[0] || false;
  var callback = arguments[1] || function() {};
  var waiting = this._watchers.length;
  var values = [];

  this.pause();

  if (!waiting) {
    this.resume();

    return callback();
  }

  this._watchers.forEach(function(watcher, i) {
    self.debugEval(watcher, null, null, function(err, value) {
      values[i] = err ? '<error>' : value;
      wait();
    });
  });

  function wait() {
    if (--waiting === 0) {
      if (verbose) self.print('Watchers:');

      self._watchers.forEach(function(watcher, i) {
        self.print(leftPad(i, ' ', self._watchers.length - 1) +
                   ': ' + watcher + ' = ' +
                   JSON.stringify(values[i]));
      });

      if (verbose) self.print('');

      self.resume();

      callback(null);
    }
  }
};

// Break on exception
Interface.prototype.breakOnException = function breakOnException() {
  if (!this.requireConnection()) return;

  var self = this;

  // Break on exceptions
  this.pause();
  this.client.reqSetExceptionBreak('all', function(err, res) {
    self.resume();
  });
};

// Add breakpoint
Interface.prototype.setBreakpoint = function(script, line,
                                             condition, silent) {
  if (!this.requireConnection()) return;

  const self = this;
  var scriptId;
  var ambiguous;

  // setBreakpoint() should insert breakpoint on current line
  if (script === undefined) {
    script = this.client.currentScript;
    line = this.client.currentSourceLine + 1;
  }

  // setBreakpoint(line-number) should insert breakpoint in current script
  if (line === undefined && typeof script === 'number') {
    line = script;
    script = this.client.currentScript;
  }

  if (script === undefined) {
    this.print('Cannot determine the current script, ' +
        'make sure the debugged process is paused.');
    return;
  }

  let req;
  if (script.endsWith('()')) {
    // setBreakpoint('functionname()');
    req = {
      type: 'function',
      target: script.replace(/\(\)$/, ''),
      condition: condition
    };
  } else {
    // setBreakpoint('scriptname')
    if (script != +script && !this.client.scripts[script]) {
      var scripts = this.client.scripts;
      for (var id in scripts) {
        if (scripts[id] &&
            scripts[id].name &&
            scripts[id].name.indexOf(script) !== -1) {
          if (scriptId) {
            ambiguous = true;
          }
          scriptId = id;
        }
      }
    } else {
      scriptId = script;
    }

    if (ambiguous) return this.error('Script name is ambiguous');
    if (line <= 0) return this.error('Line should be a positive value');

    if (scriptId) {
      req = {
        type: 'scriptId',
        target: scriptId,
        line: line - 1,
        condition: condition
      };
    } else {
      this.print('Warning: script \'' + script + '\' was not loaded yet.');
      var escapedPath = script.replace(/([/\\.?*()^${}|[\]])/g, '\\$1');
      var scriptPathRegex = '^(.*[\\/\\\\])?' + escapedPath + '$';
      req = {
        type: 'scriptRegExp',
        target: scriptPathRegex,
        line: line - 1,
        condition: condition
      };
    }
  }

  self.pause();
  self.client.setBreakpoint(req, function(err, res) {
    if (err) {
      if (!silent) {
        self.error(err);
      }
    } else {
      if (!silent) {
        self.list(5);
      }

      // Try load scriptId and line from response
      if (!scriptId) {
        scriptId = res.script_id;
        line = res.line + 1;
      }

      // Remember this breakpoint even if scriptId is not resolved yet
      self.client.breakpoints.push({
        id: res.breakpoint,
        scriptId: scriptId,
        script: (self.client.scripts[scriptId] || {}).name,
        line: line,
        condition: condition,
        scriptReq: script
      });
    }
    self.resume();
  });
};

// Clear breakpoint
Interface.prototype.clearBreakpoint = function(script, line) {
  if (!this.requireConnection()) return;

  var ambiguous;
  var breakpoint;
  var scriptId;
  var index;

  this.client.breakpoints.some(function(bp, i) {
    if (bp.scriptId === script ||
        bp.scriptReq === script ||
        (bp.script && bp.script.indexOf(script) !== -1)) {
      if (index !== undefined) {
        ambiguous = true;
      }
      scriptId = script;
      if (bp.line === line) {
        index = i;
        breakpoint = bp.id;
        return true;
      }
    }
  });

  if (!scriptId && !this.client.scripts[script]) {
    var scripts = this.client.scripts;
    for (var id in scripts) {
      if (scripts[id] &&
          scripts[id].name &&
          scripts[id].name.indexOf(script) !== -1) {
        if (scriptId) {
          ambiguous = true;
        }
        scriptId = id;
      }
    }
  }

  if (ambiguous) return this.error('Script name is ambiguous');

  if (scriptId === undefined) {
    return this.error('Script ' + script + ' not found');
  }

  if (breakpoint === undefined) {
    return this.error('Breakpoint not found on line ' + line);
  }

  var self = this;
  const req = {breakpoint};

  self.pause();
  self.client.clearBreakpoint(req, function(err, res) {
    if (err) {
      self.error(err);
    } else {
      self.client.breakpoints.splice(index, 1);
      self.list(5);
    }
    self.resume();
  });
};


// Show breakpoints
Interface.prototype.breakpoints = function() {
  if (!this.requireConnection()) return;

  this.pause();
  var self = this;
  this.client.listbreakpoints(function(err, res) {
    if (err) {
      self.error(err);
    } else {
      self.print(res);
      self.resume();
    }
  });
};


// Pause child process
Interface.prototype.pause_ = function() {
  if (!this.requireConnection()) return;

  const self = this;
  const cmd = 'process._debugPause();';

  this.pause();
  this.client.reqFrameEval(cmd, NO_FRAME, function(err, res) {
    if (err) {
      self.error(err);
    } else {
      self.resume();
    }
  });
};


// execute expression
Interface.prototype.exec = function(code) {
  this.debugEval(code, null, null, (err, result) => {
    if (err) {
      this.error(err);
    } else {
      this.print(util.inspect(result, {colors: true}));
    }
  });
};


// Kill child process
Interface.prototype.kill = function() {
  if (!this.child) return;
  this.killChild();
};


// Activate debug repl
Interface.prototype.repl = function() {
  if (!this.requireConnection()) return;

  var self = this;

  self.print('Press Ctrl + C to leave debug repl');

  // Don't display any default messages
  var listeners = this.repl.rli.listeners('SIGINT').slice(0);
  this.repl.rli.removeAllListeners('SIGINT');

  function exitDebugRepl() {
    // Restore all listeners
    process.nextTick(function() {
      listeners.forEach(function(listener) {
        self.repl.rli.on('SIGINT', listener);
      });
    });

    // Exit debug repl
    self.exitRepl();

    self.repl.rli.removeListener('SIGINT', exitDebugRepl);
    self.repl.removeListener('exit', exitDebugRepl);
  }

  // Exit debug repl on SIGINT
  this.repl.rli.on('SIGINT', exitDebugRepl);

  // Exit debug repl on repl exit
  this.repl.on('exit', exitDebugRepl);

  // Set new
  this.repl.eval = (code, ctx, file, cb) =>
    this.debugEval(code, ctx, file, cb);
  this.repl.context = {};

  // Swap history
  this.history.control = this.repl.rli.history;
  this.repl.rli.history = this.history.debug;

  this.repl.rli.setPrompt('> ');
  this.repl.displayPrompt();
};


// Exit debug repl
Interface.prototype.exitRepl = function() {
  // Restore eval
  this.repl.eval = (code, ctx, file, cb) =>
    this.controlEval(code, ctx, file, cb);

  // Swap history
  this.history.debug = this.repl.rli.history;
  this.repl.rli.history = this.history.control;

  this.repl.context = this.context;
  this.repl.rli.setPrompt('debug> ');
  this.repl.displayPrompt();
};


// Quit
Interface.prototype.quit = function() {
  this.killChild();
  process.exit(0);
};


// Kills child process
Interface.prototype.killChild = function() {
  if (this.child) {
    this.child.kill();
    this.child = null;
  }

  if (this.client) {
    // Save breakpoints
    this.breakpoints = this.client.breakpoints;

    this.client.destroy();
    this.client = null;
  }
};


// Spawns child process (and restores breakpoints)
Interface.prototype.trySpawn = function(cb) {
  const self = this;
  const breakpoints = this.breakpoints || [];
  var port = exports.port;
  var host = '127.0.0.1';
  var childArgs = this.args;

  this.killChild();
  assert(!this.child);

  var isRemote = false;
  if (this.args.length === 2) {
    const match = this.args[1].match(/^([^:]+):(\d+)$/);

    if (match) {
      // Connecting to remote debugger
      // `node debug localhost:5858`
      host = match[1];
      port = parseInt(match[2], 10);
      isRemote = true;
    }
  } else if (this.args.length === 3) {
    // `node debug -p pid`
    if (this.args[1] === '-p' && /^\d+$/.test(this.args[2])) {
      const pid = parseInt(this.args[2], 10);
      try {
        process._debugProcess(pid);
      } catch (e) {
        if (e.code === 'ESRCH') {
          internalUtil.error(`Target process: ${pid} doesn't exist.`);
          process.exit(1);
        }
        throw e;
      }
      isRemote = true;
    } else {
      const match = this.args[1].match(/^--port=(\d+)$/);
      if (match) {
        // Start debugger on custom port
        // `node debug --port=5858 app.js`
        port = parseInt(match[1], 10);
        childArgs = ['--debug-brk=' + port].concat(this.args.slice(2));
      }
    }
  }

  if (!isRemote) {
    // pipe stream into debugger
    this.child = spawn(process.execPath, childArgs);

    this.child.stdout.on('data', (text) => this.childPrint(text));
    this.child.stderr.on('data', (text) => this.childPrint(text));
  }

  this.pause();

  const client = self.client = new Client();
  var connectionAttempts = 0;

  client.once('ready', function() {
    self.stdout.write(' ok\n');

    // Restore breakpoints
    breakpoints.forEach(function(bp) {
      self.print('Restoring breakpoint ' + bp.scriptReq + ':' + bp.line);
      self.setBreakpoint(bp.scriptReq, bp.line, bp.condition, true);
    });

    client.on('close', function() {
      self.pause();
      self.print('program terminated');
      self.resume();
      self.client = null;
      self.killChild();
    });

    if (cb) cb();
    self.resume();
  });

  client.on('unhandledResponse', function(res) {
    self.pause();
    self.print('\nunhandled res:' + JSON.stringify(res));
    self.resume();
  });

  client.on('break', function(res) {
    self.handleBreak(res.body);
  });

  client.on('exception', function(res) {
    self.handleBreak(res.body);
  });

  client.on('error', connectError);
  function connectError() {
    // If it's failed to connect 10 times then print failed message
    if (connectionAttempts >= 10) {
      internalUtil.error(' failed to connect, please retry');
      process.exit(1);
    }
    setTimeout(attemptConnect, 500);
  }

  function attemptConnect() {
    ++connectionAttempts;
    self.stdout.write('.');
    client.connect(port, host);
  }

  if (isRemote) {
    self.print('connecting to ' + host + ':' + port + ' ..', true);
    attemptConnect();
  } else {
    this.child.stderr.once('data', function() {
      self.print('connecting to ' + host + ':' + port + ' ..', true);
      setImmediate(attemptConnect);
    });
  }
};
