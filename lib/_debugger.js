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

var net = require('net'),
    repl = require('repl'),
    vm = require('vm'),
    util = require('util'),
    inherits = util.inherits,
    spawn = require('child_process').spawn;

exports.port = 5858;

exports.start = function() {
  if (process.argv.length < 3) {
    console.error('Usage: node debug script.js');
    process.exit(1);
  }

  var interface = new Interface();
  process.on('uncaughtException', function(e) {
    console.error("There was an internal error in Node's debugger. " +
        'Please report this bug.');
    console.error(e.message);
    console.error(e.stack);
    if (interface.child) interface.child.kill();
    process.exit(1);
  });
};


var args = process.argv.slice(2);
args.unshift('--debug-brk');



//
// Parser/Serializer for V8 debugger protocol
// http://code.google.com/p/v8/wiki/DebuggerProtocol
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

      var lines = res.raw.slice(0, endHeaderIndex).split('\r\n');
      for (var i = 0; i < lines.length; i++) {
        var kv = lines[i].split(/: +/);
        res.headers[kv[0]] = kv[1];
      }

      this.contentLength = +res.headers['Content-Length'];
      this.bodyStartIndex = endHeaderIndex + 4;

      this.state = 'body';
      if (res.raw.length - this.bodyStartIndex < this.contentLength) break;
      // pass thru

    case 'body':
      if (res.raw.length - this.bodyStartIndex >= this.contentLength) {
        res.body =
            res.raw.slice(this.bodyStartIndex,
                          this.bodyStartIndex + this.contentLength);
        // JSON parse body?
        res.body = res.body.length ? JSON.parse(res.body) : {};

        // Done!
        this.onResponse(res);

        this._newRes(res.raw.slice(this.bodyStartIndex + this.contentLength));
      }
      break;

    default:
      throw new Error('Unknown state');
      break;
  }
};


Protocol.prototype.serialize = function(req) {
  req.type = 'request';
  req.seq = this.reqSeq++;
  var json = JSON.stringify(req);
  return 'Content-Length: ' + json.length + '\r\n\r\n' + json;
};


var NO_FRAME = -1;

function Client() {
  net.Stream.call(this);
  var protocol = this.protocol = new Protocol(this);
  this._reqCallbacks = [];
  var socket = this;

  this.currentFrame = NO_FRAME;
  this.currentSourceLine = -1;
  this.currentSource = null;
  this.handles = {};
  this.scripts = {};
  this.breakpoints = [];

  // Note that 'Protocol' requires strings instead of Buffers.
  socket.setEncoding('utf8');
  socket.on('data', function(d) {
    protocol.execute(d);
  });

  protocol.onResponse = this._onResponse.bind(this);
}
inherits(Client, net.Stream);
exports.Client = Client;


Client.prototype._addHandle = function(desc) {
  if (typeof desc != 'object' || typeof desc.handle != 'number') {
    return;
  }

  this.handles[desc.handle] = desc;

  if (desc.type == 'script') {
    this._addScript(desc);
  }
};


var natives = process.binding('natives');


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
  var cb,
      index = -1;

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

  } else if (res.body && res.body.event == 'afterCompile') {
    this._addHandle(res.body.body.script);
    handled = true;

  } else if (res.body && res.body.event == 'scriptCollected') {
    // ???
    this._removeScript(res.body.body.script);
    handled = true;

  }

  if (cb) {
    this._reqCallbacks.splice(index, 1);
    handled = true;
    cb(res.body);
  }

  if (!handled) this.emit('unhandledResponse', res.body);
};


Client.prototype.req = function(req, cb) {
  this.write(this.protocol.serialize(req));
  cb.request_seq = req.seq;
  this._reqCallbacks.push(cb);
};


Client.prototype.reqVersion = function(cb) {
  this.req({ command: 'version' } , function(res) {
    if (cb) cb(res.body.V8Version, res.running);
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

  this.req(req, function(res) {
    if (res.success) {
      for (var ref in res.body) {
        if (typeof res.body[ref] == 'object') {
          self._addHandle(res.body[ref]);
        }
      }
    }

    if (cb) cb(res);
  });
};

Client.prototype.reqScopes = function(cb) {
  var self = this,
      req = {
        command: 'scopes',
        arguments: {}
      };
  this.req(req, function(res) {
    if (!res.success) return cb(Error(res.message) || true);
    var refs = res.body.scopes.map(function(scope) {
      return scope.object.ref;
    });

    self.reqLookup(refs, function(res) {
      if (!res.success) return cb(Error(res.message) || true)

      var globals = Object.keys(res.body).map(function(key) {
        return res.body[key].properties.map(function(prop) {
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

  // Otherwise we need to get the current frame to see which scopes it has.
  this.reqBacktrace(function(bt) {
    if (!bt.frames) {
      // ??
      cb({});
      return;
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


// Finds the first scope in the array in which the epxression evals.
Client.prototype._reqFramesEval = function(expression, evalFrames, cb) {
  if (evalFrames.length == 0) {
    // Just eval in global scope.
    this.reqFrameEval(expression, NO_FRAME, cb);
    return;
  }

  var self = this;
  var i = evalFrames.shift();

  this.reqFrameEval(expression, i, function(res) {
    if (res.success) {
      if (cb) cb(res);
    } else {
      self._reqFramesEval(expression, evalFrames, cb);
    }
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

  this.req(req, function(res) {
    if (res.success) {
      self._addHandle(res.body);
    }
    if (cb) cb(res);
  });
};


// reqBacktrace(cb)
// TODO: from, to, bottom
Client.prototype.reqBacktrace = function(cb) {
  this.req({ command: 'backtrace' } , function(res) {
    if (cb) cb(res.body);
  });
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
  this.req({ command: 'scripts' } , function(res) {
    for (var i = 0; i < res.body.length; i++) {
      self._addHandle(res.body[i]);
    }
    if (cb) cb();
  });
};


Client.prototype.reqContinue = function(cb) {
  this.currentFrame = NO_FRAME;
  this.req({ command: 'continue' }, function(res) {
    if (cb) cb(res);
  });
};

Client.prototype.listbreakpoints = function(cb) {
  this.req({ command: 'listbreakpoints' }, function(res) {
    if (cb) cb(res);
  });
};

Client.prototype.setBreakpoint = function(req, cb) {
  var req = {
    command: 'setbreakpoint',
    arguments: req
  };

  this.req(req, function(res) {
    if (cb) cb(res);
  });
};

Client.prototype.clearBreakpoint = function(req, cb) {
  var req = {
    command: 'clearbreakpoint',
    arguments: req
  };

  this.req(req, function(res) {
    if (cb) cb(res);
  });
};

Client.prototype.reqSource = function(from, to, cb) {
  var req = {
    command: 'source',
    fromLine: from,
    toLine: to
  };

  this.req(req, function(res) {
    if (cb) cb(!res.success && (res.message || true), res.body);
  });
};


// client.next(1, cb);
Client.prototype.step = function(action, count, cb) {
  var req = {
    command: 'continue',
    arguments: { stepaction: action, stepcount: count }
  };

  this.currentFrame = NO_FRAME;
  this.req(req, function(res) {
    if (cb) cb(res);
  });
};


Client.prototype.mirrorObject = function(handle, depth, cb) {
  var self = this;

  var val;

  if (handle.type == 'object') {
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

    this.reqLookup(propertyRefs, function(res) {
      if (!res.success) {
        console.error('problem with reqLookup');
        if (cb) cb(handle);
        return;
      }

      var mirror,
          waiting = 1;

      if (handle.className == 'Array') {
        mirror = [];
      } else {
        mirror = {};
      }


      var keyValues = [];
      handle.properties.forEach(function(prop, i) {
        var value = res.body[prop.ref];
        var mirrorValue;
        if (value) {
          mirrorValue = value.value ? value.value : value.text;
        } else {
          mirrorValue = '[?]';
        }


        if (Array.isArray(mirror) &&
            typeof prop.name != 'number') {
          // Skip the 'length' property.
          return;
        }

        keyValues[i] = {
          name: prop.name,
          value: mirrorValue
        };
        if (value && value.handle && depth > 0) {
          waiting++;
          self.mirrorObject(value, depth - 1, function(result) {
            keyValues[i].value = result;
            waitForOthers();
          });
        }
      });

      waitForOthers();
      function waitForOthers() {
        if (--waiting === 0 && cb) {
          keyValues.forEach(function(kv) {
            mirror[kv.name] = kv.value;
          });
          cb(mirror);
        }
      };
    });
    return;
  } else if (handle.type === 'function') {
    val = function() {};
  } else if (handle.value) {
    val = handle.value;
  } else if (handle.type === 'undefined') {
    val = undefined;
  } else {
    val = handle;
  }
  process.nextTick(function() {
    cb(val);
  });
};


Client.prototype.fullTrace = function(cb) {
  var self = this;

  this.reqBacktrace(function(trace) {
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

    self.reqLookup(refs, function(res) {
      for (var i = 0; i < trace.frames.length; i++) {
        var frame = trace.frames[i];
        frame.script = res.body[frame.script.ref];
        frame.func = res.body[frame.func.ref];
        frame.receiver = res.body[frame.receiver.ref];
      }

      if (cb) cb(trace);
    });
  });
};






var commands = [
  [
    'run (r)',
    'cont (c)',
    'next (n)',
    'step (s)',
    'out (o)',
    'backtrace (bt)',
    'setBreakpoint (sb)',
    'clearBreakpoint (cb)',
  ],
  [
    'repl',
    'restart',
    'kill',
    'list',
    'scripts',
    'breakpoints',
    'version'
  ]
];


var helpMessage = 'Commands: ' + commands.map(function(group) {
  return group.join(', ');
}).join(',\n');


function SourceUnderline(sourceText, position) {
  if (!sourceText) return '';

  var head = sourceText.slice(0, position),
      tail = sourceText.slice(position);

  // Colourize char if stdout supports colours
  if (process.stdout.isTTY) {
    tail = tail.replace(/(.+?)([^\w]|$)/, '\033[32m$1\033[39m$2');
  }

  // Return source line with coloured char at `position`
  return [
    head,
    tail
  ].join('');
}


function SourceInvocation(body) {
  return body.invocationText.replace(/\n/g, '')
                            .replace(/^([^(]*)\(.*\)([^)]*)$/, '$1(...)$2');
}


function SourceInfo(body) {
  var result = 'break in ' + SourceInvocation(body);

  if (body.script) {
    if (body.script.name) {
      result += ', ' + body.script.name;
    } else {
      result += ', [unnamed]';
    }
  }
  result += ':';
  result += body.sourceLine + 1;

  return result;
}

// This class is the repl-enabled debugger interface which is invoked on
// "node debug"
function Interface() {
  var self = this,
      child;

  process.on('exit', function() {
    self.killChild();
  });

  this.repl = new repl.REPLServer('debug> ', null,
                                  this.controlEval.bind(this));

  this.repl.rli.addListener('close', function() {
    self.killed = true;
    self.killChild();
  });

  // Lift all instance methods to repl context
  var proto = Interface.prototype,
      ignored = ['pause', 'resume', 'exitRepl', 'handleBreak',
                 'requireConnection', 'killChild', 'trySpawn',
                 'controlEval', 'debugEval', 'print', 'childPrint'],
      shortcut = {
        'run': 'r',
        'cont': 'c',
        'next': 'n',
        'step': 's',
        'out': 'o',
        'backtrace': 'bt',
        'setBreakpoint': 'sb',
        'clearBreakpoint': 'cb'
      };

  function defineProperty(key, protoKey) {
    // Check arity
    if (proto[protoKey].length === 0) {
      Object.defineProperty(self.repl.context, key, {
        get: proto[protoKey].bind(self),
        enumerable: true
      });
    } else {
      self.repl.context[key] = proto[protoKey].bind(self);
    }
  };

  for (var i in proto) {
    if (proto.hasOwnProperty(i) && ignored.indexOf(i) === -1) {
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
};


// Stream control


Interface.prototype.pause = function() {
  if (this.killed || this.paused++ > 0) return false;
  this.repl.rli.pause();
  process.stdin.pause();
};

Interface.prototype.resume = function(silent) {
  if (this.killed || this.paused === 0 || --this.paused !== 0) return false;
  this.repl.rli.resume();
  if (silent !== true) {
    this.repl.displayPrompt();
  }
  process.stdin.resume();

  if (this.waiting) {
    this.waiting();
    this.waiting = null;
  }
};


// Output
Interface.prototype.print = function(text) {
  if (this.killed) return;
  if (process.stdout.isTTY) {
    process.stdout.cursorTo(0);
    process.stdout.clearLine(1);
  }
  process.stdout.write(typeof text === 'string' ? text : util.inspect(text));
  process.stdout.write('\n');
};

Interface.prototype.childPrint = function(text) {
  this.print(text.toString().split(/\r\n|\r|\n/g).filter(function(chunk) {
    return chunk;
  }).map(function(chunk) {
    return '< ' + chunk;
  }).join('\n'));
  this.repl.displayPrompt();
};

Interface.prototype.error = function(text) {
  this.print(text);
  this.resume();
};

Interface.prototype.handleBreak = function(r) {
  this.pause();

  this.client.currentSourceLine = r.sourceLine;
  this.client.currentSourceLineText = r.sourceLineText;
  this.client.currentSourceColumn = r.sourceColumn;
  this.client.currentFrame = 0;
  this.client.currentScript = r.script.name;

  this.print(SourceInfo(r));
  this.list(2);
  this.resume();
};


Interface.prototype.requireConnection = function() {
  if (!this.client) {
    this.error('App isn\'t running... Try `run` instead');
    return false;
  }
  return true;
};

Interface.prototype.controlEval = function(code, context, filename, callback) {
  try {
    var result = vm.runInContext(code, context, filename);

    if (this.paused === 0) return callback(null, result);
    this.waiting = function() {
      callback(null, result);
    };
  } catch (e) {
    callback(e);
  }
};

Interface.prototype.debugEval = function(code, context, filename, callback) {
  var self = this,
      client = this.client;

  if (code === '.scope') {
    client.reqScopes(callback);
    return;
  }

  var frame;

  if (client.currentFrame === NO_FRAME) {
    frame = NO_FRAME;
  };

  self.pause();
  client.reqFrameEval(code, frame, function(res) {
    if (!res.success) {
      if (res.message) {
        callback(res.message);
      } else {
        callback(null);
      }
      self.resume(true);
      return;
    }

    client.mirrorObject(res.body, 3, function(mirror) {
      callback(null, mirror);
      self.resume(true);
    });
  });
};


// Commands

function intChars(n) {
  // TODO dumb:
  if (n < 50) {
    return 3;
  } else if (n < 950) {
    return 4;
  } else if (n < 9950) {
    return 5;
  } else {
    return 6;
  }
}


function leftPad(n, prefix) {
  var s = n.toString();
  var nchars = intChars(n);
  var nspaces = nchars - s.length;
  for (var i = 0; i < nspaces; i++) {
    s = (prefix || ' ') + s;
  }
  return s;
}


// Print help message
Interface.prototype.help = function() {
  this.print(helpMessage);
};


// Run script
Interface.prototype.run = function() {
  if (this.child) {
    this.error('App is already running... Try `restart` instead');
  } else {
    this.trySpawn();
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
  this.client.reqVersion(function(v) {
    self.print(v);
    self.resume();
  });
};

// List source code
Interface.prototype.list = function() {
  if (!this.requireConnection()) return;

  var self = this,
      client = this.client,
      delta = arguments[0] || 5,
      from = client.currentSourceLine - delta + 1,
      to = client.currentSourceLine + delta + 1;

  self.pause();
  client.reqSource(from, to, function(err, res) {
    if (err || !res) {
      return self.error('You can\'t list source code right now');
    }

    var lines = res.source.split('\n');
    for (var i = 0; i < lines.length; i++) {
      var lineno = res.fromLine + i + 1;
      if (lineno < from || lineno > to) continue;

      if (lineno == 1) {
        // The first line needs to have the module wrapper filtered out of
        // it.
        var wrapper = require('module').wrapper[0];
        lines[i] = lines[i].slice(wrapper.length);
      }

      var breakpoint = client.breakpoints.some(function(bp) {
        return bp.script === client.currentScript &&
               bp.line == lineno;
      });

      if (lineno == 1 + client.currentSourceLine) {
        var nchars = intChars(lineno),
            pointer = breakpoint ? '*' : '=';
        for (var j = 1; j < nchars - 1; j++) {
          pointer += '=';
        }
        pointer += '>';
        self.print(pointer + ' ' +
                   SourceUnderline(lines[i], client.currentSourceColumn));
      } else {
        self.print(leftPad(lineno, breakpoint && '*') + ' ' + lines[i]);
      }
    }
    self.resume();
  });
};

// Print backtrace
Interface.prototype.backtrace = function() {
  if (!this.requireConnection()) return;

  var self = this,
      client = this.client;

  self.pause();
  client.fullTrace(function(bt) {
    if (bt.totalFrames == 0) {
      self.print('(empty stack)');
    } else {
      var trace = [],
          firstFrameNative = bt.frames[0].script.isNative;
      for (var i = 0; i < bt.frames.length; i++) {
        var frame = bt.frames[i];
        if (!firstFrameNative && frame.script.isNative) break;

        var text = '#' + i + ' ';
        if (frame.func.inferredName && frame.func.inferredName.length > 0) {
          text += frame.func.inferredName + ' ';
        }
        text += require('path').basename(frame.script.name) + ':';
        text += (frame.line + 1) + ':' + (frame.column + 1);

        trace.push(text);
      }

      self.print(trace.join('\n'));
    }
    self.resume();
  });
};


// argument full tells if it should display internal node scripts or not
Interface.prototype.scripts = function() {
  if (!this.requireConnection()) return;

  var client = this.client,
      displayNatives = arguments[0] || false,
      scripts = [];

  this.pause();
  for (var id in client.scripts) {
    var script = client.scripts[id];
    if (typeof script == 'object' && script.name) {
      if (displayNatives ||
          script.name == client.currentScript ||
          !script.isNative) {
        scripts.push(
          (script.name == client.currentScript ? '* ' : '  ') +
          id + ': ' +
          require('path').basename(script.name)
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
  this.client.reqContinue(function() {
    self.resume();
  });
};


// Step commands generator
Interface.stepGenerator = function(type, count) {
  return function() {
    if (!this.requireConnection()) return;

    var self = this;

    self.pause();
    self.client.step(type, count, function(res) {
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


// Add breakpoint
Interface.prototype.setBreakpoint = function(script, line,
                                             condition, silent) {
  if (!this.requireConnection()) return;

  var self = this,
      scriptId,
      ambiguous;

  // setBreakpoint() should insert breakpoint on current line
  if (script === undefined) {
    script = this.client.currentScript;
    line = this.client.currentSourceLine + 1;
  }

  if (script != +script && !this.client.scripts[script]) {
    Object.keys(this.client.scripts).forEach(function(id) {
      if (self.client.scripts[id].name.indexOf(script) !== -1) {
        if (scriptId) {
          ambiguous = true;
        }
        scriptId = id;
      }
    });
  } else {
    scriptId = script;
  }

  if (!scriptId) return this.error('Script : ' + script + ' not found');
  if (ambiguous) return this.error('Script name is ambiguous');
  if (line <= 0) return this.error('Line should be a positive value');

  var req = {
    type: 'scriptId',
    target: scriptId,
    line: line - 1,
    condition: condition
  };

  self.pause();
  self.client.setBreakpoint(req, function(res) {
    if (res.success) {
      if (!silent) {
        self.list(5);
      }
      self.client.breakpoints.push({
        id: res.body.breakpoint,
        scriptId: scriptId,
        script: (self.client.scripts[scriptId] || {}).name,
        line: line,
        condition: condition
      });

    } else {
      if (!silent) {
        self.print(req.message || 'error!');
      }
    }
    self.resume();
  });
};

// Clear breakpoint
Interface.prototype.clearBreakpoint = function(script, line) {
  if (!this.requireConnection()) return;

  var ambiguous,
      breakpoint,
      index;

  this.client.breakpoints.some(function(bp, i) {
    if (bp.scriptId === script || bp.script.indexOf(script) !== -1) {
      if (index !== undefined) {
        ambiguous = true;
      }
      if (bp.line === line) {
        index = i;
        breakpoint = bp.id;
        return true;
      }
    }
  });

  if (ambiguous) return this.error('Script name is ambiguous');

  if (breakpoint === undefined) {
    return this.error('Script : ' + script + ' not found');
  }

  var self = this,
      req = {
        breakpoint: breakpoint
      };

  self.pause();
  self.client.clearBreakpoint(req, function(res) {
    if (res.success) {
      self.client.breakpoints.splice(index, 1);
      self.list(5);
    } else {
      self.print(req.message || 'error!');
    }
    self.resume();
  });
};


// Show breakpoints
Interface.prototype.breakpoints = function() {
  if (!this.requireConnection()) return;

  this.pause();
  var self = this;
  this.client.listbreakpoints(function(res) {
    if (res.success) {
      self.print(res.body);
      self.resume();
    } else {
      self.error(res.message || 'Some error happened');
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
  var listeners = this.repl.rli.listeners('SIGINT');
  this.repl.rli.removeAllListeners('SIGINT');

  // Exit debug repl on Ctrl + C
  this.repl.rli.once('SIGINT', function() {
    // Restore all listeners
    process.nextTick(function() {
      listeners.forEach(function(listener) {
        self.repl.rli.on('SIGINT', listener);
      });
    });

    // Exit debug repl
    self.exitRepl();
  });

  // Set new
  this.repl.eval = this.debugEval.bind(this);
  this.repl.context = {};

  // Swap history
  this.history.control = this.repl.rli.history;
  this.repl.rli.history = this.history.debug;

  this.repl.prompt = '> ';
  this.repl.rli.setPrompt('> ');
  this.repl.displayPrompt();
};


// Exit debug repl
Interface.prototype.exitRepl = function() {
  // Restore eval
  this.repl.eval = this.controlEval.bind(this);

  // Swap history
  this.history.debug = this.repl.rli.history;
  this.repl.rli.history = this.history.control;

  this.repl.context = this.context;
  this.repl.prompt = 'debug> ';
  this.repl.rli.setPrompt('debug> ');
  this.repl.displayPrompt();
};


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


Interface.prototype.trySpawn = function(cb) {
  var self = this,
      breakpoints = this.breakpoints || [];

  this.killChild();

  this.child = spawn(process.execPath, args);

  this.child.stdout.on('data', this.childPrint.bind(this));
  this.child.stderr.on('data', this.childPrint.bind(this));

  this.pause();

  var client = self.client = new Client(),
      connectionAttempts = 0;

  client.once('ready', function() {
    process.stdout.write(' ok\n');

    // since we did debug-brk, we're hitting a break point immediately
    // continue before anything else.
    client.reqContinue(function() {
      if (cb) cb();

      // Restore breakpoints
      breakpoints.forEach(function(bp) {
        self.setBreakpoint(bp.scriptId, bp.line, bp.condition, true);
      });

      self.resume();
    });

    client.on('close', function() {
      self.pause()
      self.print('program terminated');
      self.resume();
      self.client = null;
      self.killChild();
    });
  });

  client.on('unhandledResponse', function(res) {
    self.pause();
    self.print('\nunhandled res:' + JSON.stringify(res));
    self.resume();
  });

  client.on('break', function(res) {
    self.handleBreak(res.body);
  });

  client.on('error', connectError);
  function connectError() {
    // If it's failed to connect 4 times then don't catch the next error
    if (connectionAttempts >= 4) {
      client.removeListener('error', connectError);
    }
    setTimeout(attemptConnect, 50);
  }

  function attemptConnect() {
    ++connectionAttempts;
    process.stdout.write('.');
    client.connect(exports.port);
  }

  setTimeout(function() {
    process.stdout.write('connecting..');
    attemptConnect();
  }, 50);
};
