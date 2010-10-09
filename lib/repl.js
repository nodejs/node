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

// repl.start("node > ").context.foo = "stdin is fun";  // expose foo to repl context

var sys = require('sys');
var Script = process.binding('evals').Script;
var evalcx = Script.runInContext;
var path = require("path");
var fs = require("fs");
var rl = require('readline');
var context;

var disableColors = process.env.NODE_DISABLE_COLORS ? true : false;

function cwdRequire (id) {
  if (id.match(/^\.\.\//) || id.match(/^\.\//)) {
    id = path.join(process.cwd(), id);
  }
  return require(id);
}
Object.keys(require).forEach(function (k) {
  cwdRequire[k] = require[k];
});

function resetContext() {
  context = Script.createContext();
  for (var i in global) context[i] = global[i];
  context.module = module;
  context.require = cwdRequire;
}


// Can overridden with custom print functions, such as `probe` or `eyes.js`
exports.writer = sys.inspect;

function REPLServer(prompt, stream) {
  var self = this;
  if (!context) resetContext();
  self.context = context;
  self.buffered_cmd = '';

  self.stream = stream || process.openStdin();
  self.prompt = prompt || "> ";

  var rli = self.rli = rl.createInterface(self.stream, function (text) {
    return self.complete(text);
  });

  if (rli.enabled && !disableColors) {
    // Turn on ANSI coloring.
    exports.writer = function(obj, showHidden, depth) {
      return sys.inspect(obj, showHidden, depth, true);
    }
  }

  rli.setPrompt(self.prompt);

  rli.on("SIGINT", function () {
    if (self.buffered_cmd && self.buffered_cmd.length > 0) {
      rli.write("\n");
      self.buffered_cmd = '';
      self.displayPrompt();
    } else {
      rli.close();
    }
  });

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
        // Use evalcx to supply the global context
        var ret = evalcx(self.buffered_cmd, context, "repl");
        if (ret !== undefined) {
          context._ = ret;
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
  this.rli.setPrompt(this.buffered_cmd.length ? '... ' : this.prompt);
  this.rli.prompt();
};

// read a line from the stream, then eval it
REPLServer.prototype.readline = function (cmd) {
};

/**
 * Provide a list of completions for the given leading text. This is
 * given to the readline interface for handling tab completion.
 *
 * @param {line} The text (preceding the cursor) to complete
 * @returns {Array} Two elements: (1) an array of completions; and
 *    (2) the leading text completed.
 *
 * Example:
 *  complete('var foo = sys.')
 *    -> [['sys.print', 'sys.debug', 'sys.log', 'sys.inspect', 'sys.pump'],
 *        'sys.' ]
 *
 * Warning: This eval's code like "foo.bar.baz", so it will run property
 * getter code.
 */

REPLServer.prototype.complete = function (line) {
  var completions,
      completionGroups = [],  // list of completion lists, one for each inheritance "level"
      completeOn,
      match, filter, i, j, group, c;

  // REPL commands (e.g. ".break").
  var match = null;
  match = line.match(/^\s*(\.\w*)$/);
  if (match) {
    completionGroups.push(['.break', '.clear', '.exit', '.help']);
    completeOn = match[1];
    if (match[1].length > 1) {
      filter = match[1];
    }
  }

  // require('...<Tab>')
  else if (match = line.match(/\brequire\s*\(['"](([\w\.\/-]+\/)?([\w\.\/-]*))/)) {
    //TODO: suggest require.exts be exposed to be introspec registered extensions?
    //TODO: suggest include the '.' in exts in internal repr: parity with `path.extname`.
    var exts = [".js", ".node"];
    var indexRe = new RegExp('^index(' + exts.map(regexpEscape).join('|') + ')$');

    completeOn = match[1];
    var subdir = match[2] || "";
    var filter = match[1];
    var dir, files, f, name, base, ext, abs, subfiles, s;
    group = [];
    for (i = 0; i < require.paths.length; i++) {
      dir = require.paths[i];
      if (subdir && subdir[0] === '/') {
        dir = subdir;
      } else if (subdir) {
        dir = path.join(dir, subdir);
      }
      try {
        files = fs.readdirSync(dir);
      } catch (e) {
        continue;
      }
      for (f = 0; f < files.length; f++) {
        name = files[f];
        ext = path.extname(name);
        base = name.slice(0, -ext.length);
        if (base.match(/-\d+\.\d+(\.\d+)?/) || name === ".npm") {
          // Exclude versioned names that 'npm' installs.
          continue;
        }
        if (exts.indexOf(ext) !== -1) {
          if (!subdir || base !== "index") {
            group.push(subdir + base);
          }
        } else {
          abs = path.join(dir, name);
          try {
            if (fs.statSync(abs).isDirectory()) {
              group.push(subdir + name + '/');
              subfiles = fs.readdirSync(abs);
              for (s = 0; s < subfiles.length; s++) {
                if (indexRe.test(subfiles[s])) {
                  group.push(subdir + name);
                }
              }
            }
          } catch(e) {}
        }
      }
    }
    if (group.length) {
      completionGroups.push(group);
    }

    if (!subdir) {
      // Kind of lame that this needs to be updated manually.
      // Intentionally excluding moved modules: posix, utils.
      var builtinLibs = ['assert', 'buffer', 'child_process', 'crypto', 'dgram',
        'dns', 'events', 'file', 'freelist', 'fs', 'http', 'net', 'path',
        'querystring', 'readline', 'repl', 'string_decoder', 'sys', 'tcp', 'url'];
      completionGroups.push(builtinLibs);
    }
  }

  // Handle variable member lookup.
  // We support simple chained expressions like the following (no function
  // calls, etc.). That is for simplicity and also because we *eval* that
  // leading expression so for safety (see WARNING above) don't want to
  // eval function calls.
  //
  //   foo.bar<|>     # completions for 'foo' with filter 'bar'
  //   spam.eggs.<|>  # completions for 'spam.eggs' with filter ''
  //   foo<|>         # all scope vars with filter 'foo'
  //   foo.<|>        # completions for 'foo' with filter ''
  else if (line.length === 0 || line[line.length-1].match(/\w|\./)) {
    var simpleExpressionPat = /(([a-zA-Z_]\w*)\.)*([a-zA-Z_]\w*)\.?$/;
    match = simpleExpressionPat.exec(line);
    if (line.length === 0 || match) {
      var expr;
      completeOn = (match ? match[0] : "");
      if (line.length === 0) {
        filter = "";
        expr = "";
      } else if (line[line.length-1] === '.') {
        filter = "";
        expr = match[0].slice(0, match[0].length-1);
      } else {
        var bits = match[0].split('.');
        filter = bits.pop();
        expr = bits.join('.');
      }
      //console.log("expression completion: completeOn='"+completeOn+"' expr='"+expr+"'");

      // Resolve expr and get its completions.
      var obj, memberGroups = [];
      if (!expr) {
        completionGroups.push(Object.getOwnPropertyNames(this.context));
        // Global object properties
        // (http://www.ecma-international.org/publications/standards/Ecma-262.htm)
        completionGroups.push(["NaN", "Infinity", "undefined",
          "eval", "parseInt", "parseFloat", "isNaN", "isFinite", "decodeURI",
          "decodeURIComponent", "encodeURI", "encodeURIComponent",
          "Object", "Function", "Array", "String", "Boolean", "Number",
          "Date", "RegExp", "Error", "EvalError", "RangeError",
          "ReferenceError", "SyntaxError", "TypeError", "URIError",
          "Math", "JSON"]);
        // Common keywords. Exclude for completion on the empty string, b/c
        // they just get in the way.
        if (filter) {
          completionGroups.push(["break", "case", "catch", "const",
            "continue", "debugger", "default", "delete", "do", "else", "export",
            "false", "finally", "for", "function", "if", "import", "in",
            "instanceof", "let", "new", "null", "return", "switch", "this",
            "throw", "true", "try", "typeof", "undefined", "var", "void",
            "while", "with", "yield"])
        }
      } else {
        try {
          obj = evalcx(expr, this.context, "repl");
        } catch (e) {
          //console.log("completion eval error, expr='"+expr+"': "+e);
        }
        if (obj != null) {
          if (typeof obj === "object" || typeof obj === "function") {
            memberGroups.push(Object.getOwnPropertyNames(obj));
          }
          // works for non-objects
          var p = obj.constructor ? obj.constructor.prototype : null;
          try {
            var sentinel = 5;
            while (p !== null) {
              memberGroups.push(Object.getOwnPropertyNames(p));
              p = Object.getPrototypeOf(p);
              // Circular refs possible? Let's guard against that.
              sentinel--;
              if (sentinel <= 0) {
                break;
              }
            }
          } catch (e) {
            //console.log("completion error walking prototype chain:" + e);
          }
        }

        if (memberGroups.length) {
          for (i = 0; i < memberGroups.length; i++) {
            completionGroups.push(memberGroups[i].map(function(member) {
              return expr + '.' + member;
            }));
          }
          if (filter) {
            filter = expr + '.' + filter;
          }
        }
      }
    }
  }

  // Filter, sort (within each group), uniq and merge the completion groups.
  if (completionGroups.length && filter) {
    var newCompletionGroups = [];
    for (i = 0; i < completionGroups.length; i++) {
      group = completionGroups[i].filter(function(elem) {
        return elem.indexOf(filter) == 0;
      });
      if (group.length) {
        newCompletionGroups.push(group);
      }
    }
    completionGroups = newCompletionGroups;
  }
  if (completionGroups.length) {
    var uniq = {};  // unique completions across all groups
    completions = [];
    // Completion group 0 is the "closest" (least far up the inheritance chain)
    // so we put its completions last: to be closest in the REPL.
    for (i = completionGroups.length - 1; i >= 0; i--) {
      group = completionGroups[i];
      group.sort();
      for (var j = 0; j < group.length; j++) {
        c = group[j];
        if (!uniq.hasOwnProperty(c)) {
          completions.push(c);
          uniq[c] = true;
        }
      }
      completions.push(""); // separator btwn groups
    }
    while (completions.length && completions[completions.length-1] === "") {
      completions.pop();
    }
  }

  return [completions || [], completeOn];
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
    // TODO remove me after 0.3.x
    self.buffered_cmd = '';
    self.displayPrompt();
    return true;
  case ".clear":
    self.stream.write("Clearing context...\n");
    self.buffered_cmd = '';
    resetContext();
    self.displayPrompt();
    return true;
  case ".exit":
    self.stream.destroy();
    return true;
  case ".help":
    self.stream.write(".clear\tBreak, and also clear the local context.\n");
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

function regexpEscape(s) {
  return s.replace(/[-[\]{}()*+?.,\\^$|#\s]/g, "\\$&");
}

/**
 * Converts commands that use var and function <name>() to use the
 * local exports.context when evaled. This provides a local context
 * on the REPL.
 *
 * @param {String} cmd The cmd to convert
 * @returns {String} The converted command
 */
REPLServer.prototype.convertToContext = function (cmd) {
  var self = this, matches,
    scopeVar = /^\s*var\s*([_\w\$]+)(.*)$/m,
    scopeFunc = /^\s*function\s*([_\w\$]+)/;

  // Replaces: var foo = "bar";  with: self.context.foo = bar;
  matches = scopeVar.exec(cmd);
  if (matches && matches.length === 3) {
    return "self.context." + matches[1] + matches[2];
  }

  // Replaces: function foo() {};  with: foo = function foo() {};
  matches = scopeFunc.exec(self.buffered_cmd);
  if (matches && matches.length === 2) {
    return matches[1] + " = " + self.buffered_cmd;
  }

  return cmd;
};
