var net = require('net');
var readline = require('readline');
var inherits = require('util').inherits;
var spawn = require('child_process').spawn;

exports.port = 5858;

exports.start = function() {
  var interface = new Interface();
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
  if (typeof desc != 'object' || !desc.handle) throw new Error('bad type');
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


Client.prototype.reqEval = function(expression, cb) {
  var self = this;
  var req = {
    command: 'evaluate',
    arguments: { expression: expression }
  };


  if (this.currentFrame == NO_FRAME) {
    req.arguments.global = true;
  } else {
    req.arguments.frame = this.currentFrame;
  }

  this.req(req, function(res) {
    console.error('reqEval res ', res.body);
    self._addHandle(res.body);
    if (cb) cb(res.body);
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




var helpMessage = 'Commands: run, kill, print, step, next, ' +
                  'continue, scripts, backtrace, version, quit';

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
  var term = this.term = readline.createInterface(process.stdout);
  var child;
  var client;
  var term;

  process.on('exit', function() {
    self.killChild();
  });

  this.stdin = process.openStdin();
  this.stdin.addListener('data', function(chunk) {
    term.write(chunk);
  });

  term.setPrompt('debug> ');
  term.prompt();

  this.quitting = false;

  process.on('SIGINT', function() {
    self.handleSIGINT();
  });

  term.on('SIGINT', function() {
    self.handleSIGINT();
  });

  term.on('close', function() {
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

  } else if (/^backtrace/.test(cmd) || /^bt/.test(cmd)) {
    if (!client) {
      self.printNotConnected();
      return;
    }
    client.reqBacktrace(function(bt) {
      if (/full/.test(cmd)) {
        console.log(bt);
      } else if (bt.totalFrames == 0) {
        console.log('(empty stack)');
      } else {
        var result = '';
        for (j = 0; j < bt.frames.length; j++) {
          if (j != 0) result += '\n';
          result += bt.frames[j].text;
        }
        console.log(result);
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
        if (res) {
          console.log(res.text);
        } else {
          console.log(res);
        }
        term.prompt();
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
      self.restartQuestion(cb);
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

  setTimeout(function() {
    process.stdout.write('connecting...');
    var client = self.client = new Client();
    client.connect(exports.port);

    client.once('ready', function() {
      process.stdout.write('ok\r\n');

      // since we did debug-brk, we're hitting a break point immediately
      // continue before anything else.
      client.reqContinue(function() {
        if (cb) cb();
      });
    });

    client.on('close', function() {
      console.log('\nprogram terminated');
      self.client = null;
      self.killChild();
      if (!self.quitting) self.term.prompt();
    });

    client.on('unhandledResponse', function(res) {
      console.log('\r\nunhandled res:');
      console.log(res);
      self.term.prompt();
    });

    client.on('break', function(res) {
      self.handleBreak(res.body);
    });
  }, 100);
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
        text += script.name + '\n';
      }
    }
  }
  process.stdout.write(text);
};



