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
  c.connect(exports.port, function () {
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

  this.currentFrame = NO_FRAME;
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

  if (this.currentFrame == NO_FRAME) req.arguments.global = true;

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


var helpMessage = "Commands: scripts, backtrace, version, eval, help, quit";


function startInterface() {

  var i = readline.createInterface(process.stdout);
  var stdin = process.openStdin();
  stdin.addListener('data', function(chunk) {
    i.write(chunk);
  });

  var prompt = '> ';

  i.setPrompt(prompt);
  i.prompt();

  i.on('SIGINT', function() {
    i.close();
  });

  i.on('line', function(cmd) {
    if (cmd == 'quit') {
      process.exit(0);
    } else if (/^help/.test(cmd)) {
      console.log(helpMessage);
      i.prompt();

    } else if ('version' == cmd) {
      c.reqVersion(function (v) {
        console.log(v);
        i.prompt();
      });

    } else if ('backtrace' == cmd || 'bt' == cmd) {
      c.reqBacktrace(function (bt) {
        console.log(bt);
        i.prompt();
      });

    } else if (/^scripts/.test(cmd)) {
      c.reqScripts(function (res) {
        var text = res.map(function (x) { return x.text; });
        console.log(text.join('\n'));
        i.prompt();
      });

    } else if (/^eval/.test(cmd)) {
      c.reqEval(cmd.slice(5), function (res) {
        console.log(res);
        i.prompt();
      });

    } else {
      if (!/^\s*$/.test(cmd)) {
        // If it's not all white-space print this error message.
        console.log('Unknown command "%s". Try "help"', cmd);
      }
      i.prompt();
    }
  });

  c.on('unhandledResponse', function (res) {
    console.log("\r\nunhandled res:");
    console.log(res.body);
    i.prompt();
  });

  i.on('close', function() {
    stdin.destroy();
  });
}
