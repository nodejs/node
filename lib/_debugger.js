var net = require('net');
var readline = require('readline');
var inherits = require('util').inherits;

exports.port = 5858;

exports.start = function (pid) {
  if (pid) {
    process.kill(pid, "SIGUSR1");
    setTimeout(tryConnect, 100);
  } else {
    tryConnect();
  }
};

var c;

function tryConnect() {
  c = new Client();

  process.stdout.write("connecting...");
  c.connect(exports.port);
  c.on('ready', function () {
    process.stdout.write("ok\r\n");
    startInterface();
  });

}

//
// Parser/Serializer for V8 debugger protocol
// http://code.google.com/p/v8/wiki/DebuggerProtocol
//
// Usage:
//    p = new Protocol();
//
//    p.onResponse = function (res) {
//      // do stuff with response from V8
//    };
//
//    socket.setEncoding('utf8');
//    socket.on('data', function (s) {
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
      throw new Error("Unknown state");
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
  var protocol = this.protocol = new Protocol(c);
  this._reqCallbacks = [];
  var socket = this;

  this.currentFrame = 0;
  this.currentSourceLine = -1;

  // Note that 'Protocol' requires strings instead of Buffers.
  socket.setEncoding('utf8');
  socket.on('data', function(d) {
    protocol.execute(d);
  });

  protocol.onResponse = this._onResponse.bind(this);
};
inherits(Client, net.Stream);
exports.Client = Client;


Client.prototype._onResponse = function(res) {
  for (var i = 0; i < this._reqCallbacks.length; i++) {
    var cb = this._reqCallbacks[i];
    if (this._reqCallbacks[i].request_seq == cb.request_seq) break;
  }

  if (res.headers.Type == 'connect') {
    // do nothing
    this.emit('ready');
  } else if (res.body && res.body.event == 'break') {
    this.emit('break', res.body);
  } else if (cb) {
    this._reqCallbacks.splice(i, 1);
    cb(res.body);
  } else {
    this.emit('unhandledResponse', res.body);
  }
};


Client.prototype.req = function(req, cb) {
  this.write(this.protocol.serialize(req));
  cb.request_seq = req.seq;
  this._reqCallbacks.push(cb);
};


Client.prototype.reqVersion = function(cb) {
  this.req({ command: 'version' } , function (res) {
    if (cb) cb(res.body.V8Version, res.running);
  });
};


Client.prototype.reqEval = function(expression, cb) {
  var req = {
    command: 'evaluate',
    arguments: { expression: expression }
  };

  if (this.currentFrame == NO_FRAME) {
    req.arguments.global = true;
  } else {
    req.arguments.frame = this.currentFrame;
  }

  this.req(req, function (res) {
    if (cb) cb(res.body);
  });
};


// reqBacktrace(cb)
// TODO: from, to, bottom
Client.prototype.reqBacktrace = function(cb) {
  this.req({ command: 'backtrace' } , function (res) {
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
  this.req({ command: 'scripts' } , function (res) {
    if (cb) cb(res.body);
  });
};


Client.prototype.reqContinue = function(cb) {
  this.req({ command: 'continue' }, function (res) {
    if (cb) cb(res);
  });
};

// c.next(1, cb);
Client.prototype.step = function(action, count, cb) {
  var req = {
    command: 'continue',
    arguments: { stepaction: action, stepcount: count }
  };

  this.req(req, function (res) {
    if (cb) cb(res);
  });
};




var helpMessage = "Commands: print, step, next, continue, scripts, backtrace, version, quit";

function SourceUnderline(source_text, position) {
  if (!source_text) {
    return;
  }

  // Create an underline with a caret pointing to the source position. If the
  // source contains a tab character the underline will have a tab character in
  // the same place otherwise the underline will have a space character.
  var underline = '';
  for (var i = 0; i < position; i++) {
    if (source_text[i] == '\t') {
      underline += '\t';
    } else {
      underline += ' ';
    }
  }
  underline += '^';

  // Return the source line text with the underline beneath.
  return source_text + '\n' + underline;
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
  result += ' line ';
  result += body.sourceLine + 1;
  result += ' column ';
  result += body.sourceColumn + 1;

  return result;
}





function startInterface() {

  var term = readline.createInterface(process.stdout);
  var stdin = process.openStdin();
  stdin.addListener('data', function(chunk) {
    term.write(chunk);
  });

  var prompt = 'debug> ';

  term.setPrompt('debug> ');
  term.prompt();

  var quitTried = false;

  function tryQuit() {
    if (quitTried) return;
    quitTried = true;
    term.close();
    console.log("debug done\n");
    if (c.writable) {
      c.reqContinue(function (res) {
        process.exit(0);
      });
    } else {
      process.exit(0);
    }
  }

  term.on('SIGINT', tryQuit);
  term.on('close', tryQuit);
  c.on('close', function () {
    process.exit(0);
  });

  term.on('line', function(cmd) {
    if (cmd == 'quit' || cmd == 'q' || cmd == 'exit') {
      tryQuit();
    } else if (/^help/.test(cmd)) {
      console.log(helpMessage);
      term.prompt();

    } else if ('version' == cmd) {
      c.reqVersion(function (v) {
        console.log(v);
        term.prompt();
      });

    } else if (/^backtrace/.test(cmd) || /^bt/.test(cmd)) {
      c.reqBacktrace(function (bt) {
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

    } else if (/^continue/.test(cmd) || /^c/.test(cmd)) {
      c.reqContinue(function (res) {
        // Wait for break point. (disable raw mode?)
      });

    } else if (/^next/.test(cmd) || /^n/.test(cmd)) {
      c.step('next', 1, function (res) {
        // Wait for break point. (disable raw mode?)
      });

    } else if (/^step/.test(cmd) || /^s/.test(cmd)) {
      c.step('in', 1, function (res) {
        // Wait for break point. (disable raw mode?)
      });

    } else if (/^scripts/.test(cmd)) {
      c.reqScripts(function (res) {
        if (/full/.test(cmd)) {
          console.log(res);
        } else {
          var text = res.map(function (x) { return x.text; });
          console.log(text.join('\n'));
        }
        term.prompt();
      });

    } else if (/^print/.test(cmd) || /^p/.test(cmd)) {
      var i = cmd.indexOf(' ');
      if (i < 0) {
        console.log("print [expression]");
        term.prompt();
      } else {
        cmd = cmd.slice(i);
        c.reqEval(cmd, function (res) {
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
  });

  c.on('unhandledResponse', function (res) {
    console.log("\r\nunhandled res:");
    console.log(res);
    term.prompt();
  });

  c.on('break', function (res) {
    var result = '\r\n';
    if (res.body.breakpoints) {
      result += 'breakpoint';
      if (res.body.breakpoints.length > 1) {
        result += 's';
      }
      result += ' #';
      for (var i = 0; i < res.body.breakpoints.length; i++) {
        if (i > 0) {
          result += ', #';
        }
        result += res.body.breakpoints[i];
      }
    } else {
      result += 'break';
    }
    result += ' in ';
    result += res.body.invocationText;
    result += ', ';
    result += SourceInfo(res.body);
    result += '\n';
    result += SourceUnderline(res.body.sourceLineText, res.body.sourceColumn);

    c.currentSourceLine = res.body.sourceLine;
    c.currentFrame = 0;

    console.log(result);

    term.prompt();
  });
}
