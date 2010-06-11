// A repl library that you can include in your own code to get a runtime
// interface to your program.
//
// var repl = require("/repl.js");
// repl.start("prompt> ");  // start repl on stdin
// net.createServer(function (socket) { // listen for unix socket connections and start repl on them
//   repl.start("node via Unix socket> ", socket);
// }).listen("/tmp/node-repl-sock");
// net.createServer(function (socket) { // listen for TCP socket connections and start repl on them
//   repl.start("node via TCP socket> ", socket);
// }).listen(5001);

// repl.start("node > ").scope.foo = "stdin is fun";  // expose foo to repl scope

var sys = require('sys');
var evalcx = process.binding('evals').Script.runInNewContext;
var path = require("path");
var rl = require('readline');
var scope;

function cwdRequire (id) {
  if (id.match(/^\.\.\//) || id.match(/^\.\//)) {
    id = path.join(process.cwd(), id);
  }
  return require(id);
}
Object.keys(require).forEach(function (k) {
  cwdRequire[k] = require[k];
});

function setScope (self) {
  scope = {};
  for (var i in global) scope[i] = global[i];
  scope.module = module;
  scope.require = cwdRequire;
}


// Can overridden with custom print functions, such as `probe` or `eyes.js`
exports.writer = sys.inspect;

function REPLServer(prompt, stream) {
  var self = this;
  if (!scope) setScope();
  self.scope = scope;
  self.buffered_cmd = '';

  self.stream = stream || process.openStdin();
  self.prompt = prompt || "node> ";

  var rli = self.rli = rl.createInterface(self.stream);
  rli.setPrompt(self.prompt);

  self.stream.addListener("data", function (chunk) {
    rli.write(chunk);
  });

  rli.addListener('line', function (cmd) {
    cmd = trimWhitespace(cmd);

    var flushed = true;

    // Check to see if a REPL keyword was used. If it returns true,
    // display next prompt and return.
    if (self.parseREPLKeyword(cmd) === true) return;

    // The catchall for errors
    try {
      self.buffered_cmd += cmd;
      // This try is for determining if the command is complete, or should
      // continue onto the next line.
      try {
        // Use evalcx to supply the global scope
        var ret = evalcx(self.buffered_cmd, scope, "repl");
        if (ret !== undefined) {
          scope._ = ret;
          flushed = self.stream.write(exports.writer(ret) + "\n");
        }

        self.buffered_cmd = '';
      } catch (e) {
        // instanceof doesn't work across context switches.
        if (!(e && e.constructor && e.constructor.name === "SyntaxError")) {
          throw e;
        }
      }
    } catch (e) {
      // On error: Print the error and clear the buffer
      if (e.stack) {
        flushed = self.stream.write(e.stack + "\n");
      } else {
        flushed = self.stream.write(e.toString() + "\n");
      }
      self.buffered_cmd = '';
    }

    // need to make sure the buffer is flushed before displaying the prompt
    // again. This is really ugly. Need to have callbacks from
    // net.Stream.write()
    if (flushed) {
      self.displayPrompt();
    } else {
      self.displayPromptOnDrain = true;
    }
  });

  self.stream.addListener('drain', function () {
    if (self.displayPromptOnDrain) {
      self.displayPrompt();
      self.displayPromptOnDrain = false;
    }
  });

  rli.addListener('close', function () {
    self.stream.destroy();
  });

  self.displayPrompt();
}
exports.REPLServer = REPLServer;

// prompt is a string to print on each line for the prompt,
// source is a stream to use for I/O, defaulting to stdin/stdout.
exports.start = function (prompt, source) {
  return new REPLServer(prompt, source);
};

REPLServer.prototype.displayPrompt = function () {
  this.rli.setPrompt(this.buffered_cmd.length ? '...   ' : this.prompt);
  this.rli.prompt();
};

// read a line from the stream, then eval it
REPLServer.prototype.readline = function (cmd) {
};

/**
 * Used to parse and execute the Node REPL commands.
 * 
 * @param {cmd} cmd The command entered to check
 * @returns {Boolean} If true it means don't continue parsing the command 
 */

REPLServer.prototype.parseREPLKeyword = function (cmd) {
  var self = this;
  
  switch (cmd) {
  case ".break":
    self.buffered_cmd = '';
    self.displayPrompt();
    return true;
  case ".clear":
    self.stream.write("Clearing Scope...\n");
    self.buffered_cmd = '';
    setScope();
    self.displayPrompt();
    return true;
  case ".exit":
    self.stream.destroy();
    return true;
  case ".help":
    self.stream.write(".break\tSometimes you get stuck in a place you can't get out... This will get you out.\n");
    self.stream.write(".clear\tBreak, and also clear the local scope.\n");
    self.stream.write(".exit\tExit the prompt\n");
    self.stream.write(".help\tShow repl options\n");
    self.displayPrompt();
    return true;
  }
  return false;
};

function trimWhitespace (cmd) {
  var trimmer = /^\s*(.+)\s*$/m, 
    matches = trimmer.exec(cmd);

  if (matches && matches.length === 2) {
    return matches[1];
  }
}

/**
 * Converts commands that use var and function <name>() to use the
 * local exports.scope when evaled. This provides a local scope
 * on the REPL.
 * 
 * @param {String} cmd The cmd to convert
 * @returns {String} The converted command
 */
REPLServer.prototype.convertToScope = function (cmd) {
  var self = this, matches,
    scopeVar = /^\s*var\s*([_\w\$]+)(.*)$/m,
    scopeFunc = /^\s*function\s*([_\w\$]+)/;
  
  // Replaces: var foo = "bar";  with: self.scope.foo = bar;
  matches = scopeVar.exec(cmd);
  if (matches && matches.length === 3) {
    return "self.scope." + matches[1] + matches[2];
  }
  
  // Replaces: function foo() {};  with: foo = function foo() {};
  matches = scopeFunc.exec(self.buffered_cmd);
  if (matches && matches.length === 2) {
    return matches[1] + " = " + self.buffered_cmd;
  }
  
  return cmd;
};
