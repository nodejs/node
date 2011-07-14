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

var net = require('net');
var readline = require('readline');
var inherits = require('util').inherits;
var spawn = require('child_process').spawn;

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
  for (var i = 0; i < this._reqCallbacks.length; i++) {
    var cb = this._reqCallbacks[i];
    if (this._reqCallbacks[i].request_seq == res.body.request_seq) break;
  }

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
    this._reqCallbacks.splice(i, 1);
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
  this.req({ command: 'continue' }, function(res) {
    if (cb) cb(res);
  });
};

Client.prototype.listbreakpoints = function(cb) {
  this.req({ command: 'listbreakpoints' }, function(res) {
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
    if (cb) cb(res.body);
  });
};


// client.next(1, cb);
Client.prototype.step = function(action, count, cb) {
  var req = {
    command: 'continue',
    arguments: { stepaction: action, stepcount: count }
  };

  this.req(req, function(res) {
    if (cb) cb(res);
  });
};


Client.prototype.mirrorObject = function(handle, cb) {
  var self = this;

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

      var mirror;
      if (handle.className == 'Array') {
        mirror = [];
      } else {
        mirror = {};
      }

      for (var i = 0; i < handle.properties.length; i++) {
        var value = res.body[handle.properties[i].ref];
        var mirrorValue;
        if (value) {
          mirrorValue = value.value ? value.value : value.text;
        } else {
          mirrorValue = '[?]';
        }


        if (Array.isArray(mirror) &&
            typeof handle.properties[i].name != 'number') {
          // Skip the 'length' property.
          continue;
        }

        mirror[handle.properties[i].name] = mirrorValue;
      }

      if (cb) cb(mirror);
    });

  } else if (handle.value) {
    process.nextTick(function() {
      cb(handle.value);
    });

  } else {
    process.nextTick(function() {
      cb(handle);
    });
  }
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
  'backtrace',
  'continue',
  'help',
  'info breakpoints',
  'kill',
  'list',
  'next',
  'print',
  'quit',
  'run',
  'scripts',
  'step',
  'version'
];


var helpMessage = 'Commands: ' + commands.join(', ');


function SourceUnderline(sourceText, position) {
  if (!sourceText) return;

  // Create an underline with a caret pointing to the source position. If the
  // source contains a tab character the underline will have a tab character in
  // the same place otherwise the underline will have a space character.
  var underline = '';
  for (var i = 0; i < position; i++) {
    if (sourceText[i] == '\t') {
      underline += '\t';
    } else {
      underline += ' ';
    }
  }
  underline += '^';

  // Return the source line text with the underline beneath.
  return sourceText + '\n' + underline;
}


function SourceInfo(body) {
  var result = '';

  if (body.script) {
    if (body.script.name) {
      result += body.script.name;
    } else {
      result += '[unnamed]';
    }
  }
  result += ':';
  result += body.sourceLine + 1;

  return result;
}


// This class is the readline-enabled debugger interface which is invoked on
// "node debug"
function Interface() {
  var self = this;
  var child;
  var client;

  function complete(line) {
    return self.complete(line);
  }

  var term = readline.createInterface(process.stdin, process.stdout, complete);
  this.term = term;

  process.on('exit', function() {
    self.killChild();
  });

  this.stdin = process.openStdin();

  term.setPrompt('debug> ');
  term.prompt();

  this.quitting = false;

  process.on('SIGINT', function() {
    self.handleSIGINT();
  });

  term.on('SIGINT', function() {
    self.handleSIGINT();
  });

  term.on('attemptClose', function() {
    self.tryQuit();
  });

  term.on('line', function(cmd) {
    // trim whitespace
    cmd = cmd.replace(/^\s*/, '').replace(/\s*$/, '');

    if (cmd.length) {
      self._lastCommand = cmd;
      self.handleCommand(cmd);
    } else {
      self.handleCommand(self._lastCommand);
    }
  });
}


Interface.prototype.complete = function(line) {
  // Match me with a command.
  var matches = [];
  // Remove leading whitespace
  line = line.replace(/^\s*/, '');

  for (var i = 0; i < commands.length; i++) {
    if (commands[i].indexOf(line) === 0) {
      matches.push(commands[i]);
    }
  }

  return [matches, line];
};


Interface.prototype.handleSIGINT = function() {
  if (this.paused) {
    this.child.kill('SIGINT');
  } else {
    this.tryQuit();
  }
};


Interface.prototype.quit = function() {
  if (this.quitting) return;
  this.quitting = true;
  this.killChild();
  this.term.close();
  process.exit(0);
};


Interface.prototype.tryQuit = function() {
  var self = this;

  if (self.child) {
    self.quitQuestion(function(yes) {
      if (yes) {
        self.quit();
      } else {
        self.term.prompt();
      }
    });
  } else {
    self.quit();
  }
};


Interface.prototype.pause = function() {
  this.paused = true;
  this.stdin.pause();
  this.term.pause();
};


Interface.prototype.resume = function() {
  if (!this.paused) return false;
  this.paused = false;
  this.stdin.resume();
  this.term.resume();
  this.term.prompt();
  return true;
};


Interface.prototype.handleBreak = function(r) {
  var result = '';
  if (r.breakpoints) {
    result += 'breakpoint';
    if (r.breakpoints.length > 1) {
      result += 's';
    }
    result += ' #';
    for (var i = 0; i < r.breakpoints.length; i++) {
      if (i > 0) {
        result += ', #';
      }
      result += r.breakpoints[i];
    }
  } else {
    result += 'break';
  }
  result += ' in ';
  result += r.invocationText;
  result += ', ';
  result += SourceInfo(r);
  result += '\n';
  result += SourceUnderline(r.sourceLineText, r.sourceColumn);

  this.client.currentSourceLine = r.sourceLine;
  this.client.currentFrame = 0;
  this.client.currentScript = r.script.name;

  console.log(result);

  if (!this.resume()) this.term.prompt();
};


function intChars(n) {
  // TODO dumb:
  if (n < 50) {
    return 2;
  } else if (n < 950) {
    return 3;
  } else if (n < 9950) {
    return 4;
  } else {
    return 5;
  }
}


function leftPad(n) {
  var s = n.toString();
  var nchars = intChars(n);
  var nspaces = nchars - s.length;
  for (var i = 0; i < nspaces; i++) {
    s = ' ' + s;
  }
  return s;
}


Interface.prototype.handleCommand = function(cmd) {
  var self = this;

  var client = this.client;
  var term = this.term;

  if (cmd == 'quit' || cmd == 'q' || cmd == 'exit') {
    self._lastCommand = null;
    self.tryQuit();

  } else if (/^r(un)?/.test(cmd)) {
    self._lastCommand = null;
    if (self.child) {
      self.restartQuestion(function(yes) {
        if (!yes) {
          self._lastCommand = null;
          term.prompt();
        } else {
          console.log('restarting...');
          self.killChild();
          // XXX need to wait a little bit for the restart to work?
          setTimeout(function() {
            self.trySpawn();
          }, 1000);
        }
      });
    } else {
      self.trySpawn();
    }

  } else if (/^help/.test(cmd)) {
    console.log(helpMessage);
    term.prompt();

  } else if ('version' == cmd) {
    if (!client) {
      self.printNotConnected();
      return;
    }
    client.reqVersion(function(v) {
      console.log(v);
      term.prompt();
    });

  } else if (/info +breakpoints/.test(cmd)) {
    if (!client) {
      self.printNotConnected();
      return;
    }
    client.listbreakpoints(function(res) {
      console.log(res);
      term.prompt();
    });


  } else if ('l' == cmd || 'list' == cmd) {
    if (!client) {
      self.printNotConnected();
      return;
    }

    var from = client.currentSourceLine - 5;
    var to = client.currentSourceLine + 5;

    client.reqSource(from, to, function(res) {
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

        if (lineno == 1 + client.currentSourceLine) {
          var nchars = intChars(lineno);
          var pointer = '';
          for (var j = 0; j < nchars - 1; j++) {
            pointer += '=';
          }
          pointer += '>';
          console.log(pointer + ' ' + lines[i]);
        } else {
          console.log(leftPad(lineno) + ' ' + lines[i]);
        }
      }
      term.prompt();
    });

  } else if (/^backtrace/.test(cmd) || /^bt/.test(cmd)) {
    if (!client) {
      self.printNotConnected();
      return;
    }

    client.fullTrace(function(bt) {
      if (bt.totalFrames == 0) {
        console.log('(empty stack)');
      } else {
        var text = '';
        var firstFrameNative = bt.frames[0].script.isNative;
        for (var i = 0; i < bt.frames.length; i++) {
          var frame = bt.frames[i];
          if (!firstFrameNative && frame.script.isNative) break;

          text += '#' + i + ' ';
          if (frame.func.inferredName && frame.func.inferredName.length > 0) {
            text += frame.func.inferredName + ' ';
          }
          text += require('path').basename(frame.script.name) + ':';
          text += (frame.line + 1) + ':' + (frame.column + 1);
          text += '\n';
        }

        console.log(text);
      }
      term.prompt();
    });

  } else if (cmd == 'scripts' || cmd == 'scripts full') {
    if (!client) {
      self.printNotConnected();
      return;
    }
    self.printScripts(cmd.indexOf('full') > 0);
    term.prompt();

  } else if (/^c(ontinue)?/.test(cmd)) {
    if (!client) {
      self.printNotConnected();
      return;
    }

    self.pause();
    client.reqContinue(function() {
      self.resume();
    });

  } else if (/^k(ill)?/.test(cmd)) {
    if (!client) {
      self.printNotConnected();
      return;
    }
    // kill
    if (self.child) {
      self.killQuestion(function(yes) {
        if (yes) {
          self.killChild();
        } else {
          self._lastCommand = null;
        }
      });
    } else {
      self.term.prompt();
    }

  } else if (/^next/.test(cmd) || /^n/.test(cmd)) {
    if (!client) {
      self.printNotConnected();
      return;
    }
    client.step('next', 1, function(res) {
      // Wait for break point. (disable raw mode?)
    });

  } else if (/^step/.test(cmd) || /^s/.test(cmd)) {
    if (!client) {
      self.printNotConnected();
      return;
    }
    client.step('in', 1, function(res) {
      // Wait for break point. (disable raw mode?)
    });

  } else if (/^print/.test(cmd) || /^p/.test(cmd)) {
    if (!client) {
      self.printNotConnected();
      return;
    }
    var i = cmd.indexOf(' ');
    if (i < 0) {
      console.log('print [expression]');
      term.prompt();
    } else {
      cmd = cmd.slice(i);
      client.reqEval(cmd, function(res) {
        if (!res.success) {
          console.log(res.message);
          term.prompt();
          return;
        }

        client.mirrorObject(res.body, function(mirror) {
          console.log(mirror);
          term.prompt();
        });
      });
    }

  } else {
    if (!/^\s*$/.test(cmd)) {
      // If it's not all white-space print this error message.
      console.log('Unknown command "%s". Try "help"', cmd);
    }
    term.prompt();
  }
};



Interface.prototype.yesNoQuestion = function(prompt, cb) {
  var self = this;
  self.resume();
  this.term.question(prompt, function(answer) {
    if (/^y(es)?$/i.test(answer)) {
      cb(true);
    } else if (/^n(o)?$/i.test(answer)) {
      cb(false);
    } else {
      console.log('Please answer y or n.');
      self.yesNoQuestion(prompt, cb);
    }
  });
};


Interface.prototype.restartQuestion = function(cb) {
  this.yesNoQuestion('The program being debugged has been started already.\n' +
                     'Start it from the beginning? (y or n) ', cb);
};


Interface.prototype.killQuestion = function(cb) {
  this.yesNoQuestion('Kill the program being debugged? (y or n) ', cb);
};


Interface.prototype.quitQuestion = function(cb) {
  this.yesNoQuestion('A debugging session is active. Quit anyway? (y or n) ',
                     cb);
};


Interface.prototype.killChild = function() {
  if (this.child) {
    this.child.kill();
    this.child = null;
  }

  if (this.client) {
    this.client.destroy();
    this.client = null;
  }

  this.resume();
};


Interface.prototype.trySpawn = function(cb) {
  var self = this;

  this.killChild();

  this.child = spawn(process.execPath, args, { customFds: [0, 1, 2] });


  this.pause();

  var client = self.client = new Client();
  var connectionAttempts = 0;

  client.once('ready', function() {
    process.stdout.write(' ok\r\n');

    // since we did debug-brk, we're hitting a break point immediately
    // continue before anything else.
    client.reqContinue(function() {
      if (cb) cb();
    });

    client.on('close', function() {
      console.log('\nprogram terminated');
      self.client = null;
      self.killChild();
      if (!self.quitting) self.term.prompt();
    });
  });

  client.on('unhandledResponse', function(res) {
    console.log('\r\nunhandled res:');
    console.log(res);
    self.term.prompt();
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


Interface.prototype.printNotConnected = function() {
  console.log("Program not running. Try 'run'.");
  this.term.prompt();
};


// argument full tells if it should display internal node scripts or not
Interface.prototype.printScripts = function(displayNatives) {
  var client = this.client;
  var text = '';
  for (var id in client.scripts) {
    var script = client.scripts[id];
    if (typeof script == 'object' && script.name) {
      if (displayNatives ||
          script.name == client.currentScript ||
          !script.isNative) {
        text += script.name == client.currentScript ? '* ' : '  ';
        text += require('path').basename(script.name) + '\n';
      }
    }
  }
  process.stdout.write(text);
};



