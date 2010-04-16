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

// Can overridden with custom print functions, such as `probe` or `eyes.js`
exports.writer = sys.inspect;

function REPLServer(prompt, stream) {
  var self = this;
  
  self.scope = {};
  self.buffered_cmd = '';
  self.prompt = prompt || "node> ";
  self.stream = stream || process.openStdin();
  self.stream.setEncoding('utf8');
  self.stream.addListener("data", function (chunk) {
    self.readline.call(self, chunk);
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
  var self = this;
  self.stream.write(self.buffered_cmd.length ? '...   ' : self.prompt);
};

// read a line from the stream, then eval it
REPLServer.prototype.readline = function (cmd) {
  var self = this;

  cmd = self.trimWhitespace(cmd);
  
  // Check to see if a REPL keyword was used. If it returns true,
  // display next prompt and return.
  if (self.parseREPLKeyword(cmd) === true) {
    return;
  }
  
  // The catchall for errors
  try {
    self.buffered_cmd += cmd;
    // This try is for determining if the command is complete, or should
    // continue onto the next line.
    try {
      self.buffered_cmd = self.convertToScope(self.buffered_cmd);
      
      // Scope the readline with self.scope to provide "local" vars and make Douglas Crockford cry
      with (self.scope) {
        var ret = eval(self.buffered_cmd);
        if (ret !== undefined) {
          self.scope['_'] = ret;
          self.stream.write(exports.writer(ret) + "\n");
        }
      }
        
      self.buffered_cmd = '';
    } catch (e) {
      if (!(e instanceof SyntaxError)) throw e;
    }
  } catch (e) {
    // On error: Print the error and clear the buffer
    if (e.stack) {
      self.stream.write(e.stack + "\n");
    } else {
      self.stream.write(e.toString() + "\n");
    }
    self.buffered_cmd = '';
  }
  
  self.displayPrompt();
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
    self.scope = {};
    self.displayPrompt();
    return true;
  case ".exit":
    self.stream.end();
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

/**
 * Trims Whitespace from a line.
 * 
 * @param {String} cmd The string to trim the whitespace from
 * @returns {String} The trimmed string 
 */
REPLServer.prototype.trimWhitespace = function (cmd) {
  var trimmer = /^\s*(.+)\s*$/m, 
    matches = trimmer.exec(cmd);

  if (matches && matches.length === 2) {
    return matches[1];
  }
};

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
