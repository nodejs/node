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

/* A repl library that you can include in your own code to get a runtime
 * interface to your program.
 *
 *   var repl = require("/repl.js");
 *   // start repl on stdin
 *   repl.start("prompt> ");
 *
 *   // listen for unix socket connections and start repl on them
 *   net.createServer(function (socket) {
 *     repl.start("node via Unix socket> ", socket);
 *   }).listen("/tmp/node-repl-sock");
 *
 *   // listen for TCP socket connections and start repl on them
 *   net.createServer(function (socket) {
 *     repl.start("node via TCP socket> ", socket);
 *   }).listen(5001);
 *
 *   // expose foo to repl context
 *   repl.start("node > ").context.foo = "stdin is fun";
 */

var util = require('util');
var vm = require('vm');
var path = require('path');
var fs = require('fs');
var rl = require('readline');

// If obj.hasOwnProperty has been overridden, then calling
// obj.hasOwnProperty(prop) will break.
// See: https://github.com/joyent/node/issues/1707
function hasOwnProperty(obj, prop) {
  return Object.prototype.hasOwnProperty.call(obj, prop);
}


var context;

exports.disableColors = process.env.NODE_DISABLE_COLORS ? true : false;

// hack for require.resolve("./relative") to work properly.
module.filename = process.cwd() + '/repl';

// hack for repl require to work properly with node_modules folders
module.paths = require('module')._nodeModulePaths(module.filename);

// Can overridden with custom print functions, such as `probe` or `eyes.js`
exports.writer = util.inspect;


function REPLServer(prompt, stream, eval, useGlobal, ignoreUndefined) {
  var self = this;

  self.useGlobal = useGlobal;

  self.eval = eval || function(code, context, file, cb) {
    var err, result;
    try {
      if (useGlobal) {
        result = vm.runInThisContext(code, file);
      } else {
        result = vm.runInContext(code, context, file);
      }
    } catch (e) {
      err = e;
    }
    cb(err, result);
  };

  self.resetContext();
  self.bufferedCommand = '';

  if (stream) {
    // We're given a duplex socket
    if (stream.stdin || stream.stdout) {
      self.outputStream = stream.stdout;
      self.inputStream = stream.stdin;
    } else {
      self.outputStream = stream;
      self.inputStream = stream;
    }
  } else {
    self.outputStream = process.stdout;
    self.inputStream = process.stdin;
    process.stdin.resume();
  }

  self.prompt = (prompt != undefined ? prompt : '> ');

  function complete(text, callback) {
    self.complete(text, callback);
  }

  var rli = rl.createInterface(self.inputStream, self.outputStream, complete);
  self.rli = rli;

  this.commands = {};
  defineDefaultCommands(this);

  if (rli.enabled && !exports.disableColors &&
      exports.writer === util.inspect) {
    // Turn on ANSI coloring.
    exports.writer = function(obj, showHidden, depth) {
      return util.inspect(obj, showHidden, depth, true);
    };
  }

  rli.setPrompt(self.prompt);

  var sawSIGINT = false;
  rli.on('SIGINT', function() {
    if (sawSIGINT) {
      rli.close();
      process.exit();
    }

    rli.line = '';

    if (!(self.bufferedCommand && self.bufferedCommand.length > 0) &&
        rli.line.length === 0) {
      rli.output.write('\n(^C again to quit)\n');
      sawSIGINT = true;
    } else {
      rli.output.write('\n');
    }

    self.bufferedCommand = '';
    self.displayPrompt();
  });

  rli.addListener('line', function(cmd) {
    sawSIGINT = false;
    var skipCatchall = false;
    cmd = trimWhitespace(cmd);

    // Check to see if a REPL keyword was used. If it returns true,
    // display next prompt and return.
    if (cmd && cmd.charAt(0) === '.') {
      var matches = cmd.match(/^(\.[^\s]+)\s*(.*)$/);
      var keyword = matches && matches[1];
      var rest = matches && matches[2];
      if (self.parseREPLKeyword(keyword, rest) === true) {
        return;
      } else {
        self.outputStream.write('Invalid REPL keyword\n');
        skipCatchall = true;
      }
    }

    if (!skipCatchall) {
      var evalCmd = self.bufferedCommand + cmd + '\n';

      // This try is for determining if the command is complete, or should
      // continue onto the next line.
      // We try to evaluate both expressions e.g.
      //  '{ a : 1 }'
      // and statements e.g.
      //  'for (var i = 0; i < 10; i++) console.log(i);'

      // First we attempt to eval as expression with parens.
      // This catches '{a : 1}' properly.
      self.eval('(' + evalCmd + ')',
                self.context,
                'repl',
                function(e, ret) {
            if (e && !isSyntaxError(e)) return finish(e);

            if (typeof ret === 'function' || e) {
              // Now as statement without parens.
              self.eval(evalCmd, self.context, 'repl', finish);
            } else {
              finish(null, ret);
            }
          });

    } else {
      finish(null);
    }

    function isSyntaxError(e) {
      // Convert error to string
      e = e && (e.stack || e.toString());
      return e && e.match(/^SyntaxError/) &&
             !(e.match(/^SyntaxError: Unexpected token .*\n/) &&
             e.match(/\n    at Object.parse \(native\)\n/));
    }

    function finish(e, ret) {

      self.memory(cmd);

      // If error was SyntaxError and not JSON.parse error
      if (isSyntaxError(e)) {
        // Start buffering data like that:
        // {
        // ...  x: 1
        // ... }
        self.bufferedCommand += cmd + '\n';
        self.displayPrompt();
        return;
      } else if (e) {
        self.outputStream.write((e.stack || e) + '\n');
      }

      // Clear buffer if no SyntaxErrors
      self.bufferedCommand = '';

      // If we got any output - print it (if no error)
      if (!e && (!ignoreUndefined || ret !== undefined)) {
        self.context._ = ret;
        self.outputStream.write(exports.writer(ret) + '\n');
      }

      // Display prompt again
      self.displayPrompt();
    };
  });

  rli.addListener('close', function() {
    self.inputStream.destroy();
  });

  self.displayPrompt();
}
exports.REPLServer = REPLServer;


// prompt is a string to print on each line for the prompt,
// source is a stream to use for I/O, defaulting to stdin/stdout.
exports.start = function(prompt, source, eval, useGlobal, ignoreUndefined) {
  var repl = new REPLServer(prompt, source, eval, useGlobal, ignoreUndefined);
  if (!exports.repl) exports.repl = repl;
  return repl;
};


REPLServer.prototype.createContext = function() {
  if (!this.useGlobal) {
    var context = vm.createContext();
    for (var i in global) context[i] = global[i];
  } else {
    var context = global;
  }

  context.module = module;
  context.require = require;
  context.global = context;
  context.global.global = context;

  this.lines = [];
  this.lines.level = [];

  return context;
};

REPLServer.prototype.resetContext = function(force) {
  if (!context || force) {
    context = this.createContext();
    for (var i in require.cache) delete require.cache[i];
  }

  this.context = context;
};

REPLServer.prototype.displayPrompt = function() {
  this.rli.setPrompt(this.bufferedCommand.length ?
                 '...' + new Array(this.lines.level.length).join('..') + ' ' :
                 this.prompt);
  this.rli.prompt();
};


// read a line from the stream, then eval it
REPLServer.prototype.readline = function(cmd) {
};

// A stream to push an array into a REPL
// used in REPLServer.complete
function ArrayStream() {
  this.run = function (data) {
    var self = this;
    data.forEach(function (line) {
      self.emit('data', line);
    });
  }
}
util.inherits(ArrayStream, require('stream').Stream);
ArrayStream.prototype.readable = true;
ArrayStream.prototype.writable = true;
ArrayStream.prototype.resume = function () {};
ArrayStream.prototype.write = function () {};

var requireRE = /\brequire\s*\(['"](([\w\.\/-]+\/)?([\w\.\/-]*))/;
var simpleExpressionRE =
    /(([a-zA-Z_$](?:\w|\$)*)\.)*([a-zA-Z_$](?:\w|\$)*)\.?$/;


// Provide a list of completions for the given leading text. This is
// given to the readline interface for handling tab completion.
//
// Example:
//  complete('var foo = util.')
//    -> [['util.print', 'util.debug', 'util.log', 'util.inspect', 'util.pump'],
//        'util.' ]
//
// Warning: This eval's code like "foo.bar.baz", so it will run property
// getter code.
REPLServer.prototype.complete = function(line, callback) {
  // There may be local variables to evaluate, try a nested REPL
  if (this.bufferedCommand != undefined && this.bufferedCommand.length) {
    // Get a new array of inputed lines
    var tmp = this.lines.slice();
    // Kill off all function declarations to push all local variables into
    // global scope
    this.lines.level.forEach(function (kill) {
      if (kill.isFunction) {
        tmp[kill.line] = '';
      }
    });
    var flat = new ArrayStream();         // make a new "input" stream
    var magic = new REPLServer('', flat); // make a nested REPL
    magic.context = magic.createContext();
    flat.run(tmp);                        // eval the flattened code
    // all this is only profitable if the nested REPL
    // does not have a bufferedCommand
    if (!magic.bufferedCommand) {
      return magic.complete(line, callback);
    }
  }

  var completions;

  // list of completion lists, one for each inheritance "level"
  var completionGroups = [];

  var completeOn, match, filter, i, j, group, c;

  // REPL commands (e.g. ".break").
  var match = null;
  match = line.match(/^\s*(\.\w*)$/);
  if (match) {
    completionGroups.push(Object.keys(this.commands));
    completeOn = match[1];
    if (match[1].length > 1) {
      filter = match[1];
    }

    completionGroupsLoaded();
  } else if (match = line.match(requireRE)) {
    // require('...<Tab>')
    //TODO: suggest require.exts be exposed to be introspec registered
    //extensions?
    //TODO: suggest include the '.' in exts in internal repr: parity with
    //`path.extname`.
    var exts = ['.js', '.node'];
    var indexRe = new RegExp('^index(' + exts.map(regexpEscape).join('|') +
                             ')$');

    completeOn = match[1];
    var subdir = match[2] || '';
    var filter = match[1];
    var dir, files, f, name, base, ext, abs, subfiles, s;
    group = [];
    var paths = module.paths.concat(require('module').globalPaths);
    for (i = 0; i < paths.length; i++) {
      dir = path.resolve(paths[i], subdir);
      try {
        files = fs.readdirSync(dir);
      } catch (e) {
        continue;
      }
      for (f = 0; f < files.length; f++) {
        name = files[f];
        ext = path.extname(name);
        base = name.slice(0, -ext.length);
        if (base.match(/-\d+\.\d+(\.\d+)?/) || name === '.npm') {
          // Exclude versioned names that 'npm' installs.
          continue;
        }
        if (exts.indexOf(ext) !== -1) {
          if (!subdir || base !== 'index') {
            group.push(subdir + base);
          }
        } else {
          abs = path.resolve(dir, name);
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
          } catch (e) {}
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
        'dns', 'events', 'file', 'freelist', 'fs', 'http', 'net', 'os', 'path',
        'querystring', 'readline', 'repl', 'string_decoder', 'util', 'tcp',
        'url'];
      completionGroups.push(builtinLibs);
    }

    completionGroupsLoaded();

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
  } else if (line.length === 0 || line[line.length - 1].match(/\w|\.|\$/)) {
    match = simpleExpressionRE.exec(line);
    if (line.length === 0 || match) {
      var expr;
      completeOn = (match ? match[0] : '');
      if (line.length === 0) {
        filter = '';
        expr = '';
      } else if (line[line.length - 1] === '.') {
        filter = '';
        expr = match[0].slice(0, match[0].length - 1);
      } else {
        var bits = match[0].split('.');
        filter = bits.pop();
        expr = bits.join('.');
      }

      // console.log("expression completion: completeOn='" + completeOn +
      //             "' expr='" + expr + "'");

      // Resolve expr and get its completions.
      var obj, memberGroups = [];
      if (!expr) {
        // If context is instance of vm.ScriptContext
        // Get global vars synchronously
        if (this.useGlobal ||
            this.context.constructor &&
            this.context.constructor.name === 'Context') {
          completionGroups.push(Object.getOwnPropertyNames(this.context));
          addStandardGlobals();
          completionGroupsLoaded();
        } else {
          this.eval('.scope', this.context, 'repl', function(err, globals) {
            if (err || !globals) {
              addStandardGlobals();
            } else if (Array.isArray(globals[0])) {
              // Add grouped globals
              globals.forEach(function(group) {
                completionGroups.push(group);
              });
            } else {
              completionGroups.push(globals);
              addStandardGlobals();
            }
            completionGroupsLoaded();
          });
        }

        function addStandardGlobals() {
          // Global object properties
          // (http://www.ecma-international.org/publications/standards/Ecma-262.htm)
          completionGroups.push(['NaN', 'Infinity', 'undefined',
            'eval', 'parseInt', 'parseFloat', 'isNaN', 'isFinite', 'decodeURI',
            'decodeURIComponent', 'encodeURI', 'encodeURIComponent',
            'Object', 'Function', 'Array', 'String', 'Boolean', 'Number',
            'Date', 'RegExp', 'Error', 'EvalError', 'RangeError',
            'ReferenceError', 'SyntaxError', 'TypeError', 'URIError',
            'Math', 'JSON']);
          // Common keywords. Exclude for completion on the empty string, b/c
          // they just get in the way.
          if (filter) {
            completionGroups.push(['break', 'case', 'catch', 'const',
              'continue', 'debugger', 'default', 'delete', 'do', 'else',
              'export', 'false', 'finally', 'for', 'function', 'if',
              'import', 'in', 'instanceof', 'let', 'new', 'null', 'return',
              'switch', 'this', 'throw', 'true', 'try', 'typeof', 'undefined',
              'var', 'void', 'while', 'with', 'yield']);
          }
        }

      } else {
        this.eval(expr, this.context, 'repl', function(e, obj) {
          // if (e) console.log(e);

          if (obj != null) {
            if (typeof obj === 'object' || typeof obj === 'function') {
              memberGroups.push(Object.getOwnPropertyNames(obj));
            }
            // works for non-objects
            try {
              var sentinel = 5;
              var p;
              if (typeof obj == 'object') {
                p = Object.getPrototypeOf(obj);
              } else {
                p = obj.constructor ? obj.constructor.prototype : null;
              }
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

          completionGroupsLoaded();
        });
      }
    } else {
      completionGroupsLoaded();
    }
  }

  // Will be called when all completionGroups are in place
  // Useful for async autocompletion
  function completionGroupsLoaded(err) {
    if (err) throw err;

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
      // Completion group 0 is the "closest"
      // (least far up the inheritance chain)
      // so we put its completions last: to be closest in the REPL.
      for (i = completionGroups.length - 1; i >= 0; i--) {
        group = completionGroups[i];
        group.sort();
        for (var j = 0; j < group.length; j++) {
          c = group[j];
          if (!hasOwnProperty(c)) {
            completions.push(c);
            uniq[c] = true;
          }
        }
        completions.push(''); // separator btwn groups
      }
      while (completions.length && completions[completions.length - 1] === '') {
        completions.pop();
      }
    }

    callback(null, [completions || [], completeOn]);
  }
};


/**
 * Used to parse and execute the Node REPL commands.
 *
 * @param {keyword} keyword The command entered to check.
 * @return {Boolean} If true it means don't continue parsing the command.
 */
REPLServer.prototype.parseREPLKeyword = function(keyword, rest) {
  var cmd = this.commands[keyword];
  if (cmd) {
    cmd.action.call(this, rest);
    return true;
  }
  return false;
};


REPLServer.prototype.defineCommand = function(keyword, cmd) {
  if (typeof cmd === 'function') {
    cmd = {action: cmd};
  } else if (typeof cmd.action !== 'function') {
    throw new Error('bad argument, action must be a function');
  }
  this.commands['.' + keyword] = cmd;
};

REPLServer.prototype.memory = function memory (cmd) {
  var self = this;

  self.lines = self.lines || [];
  self.lines.level = self.lines.level || [];

  // save the line so I can do magic later
  if (cmd) {
    // TODO should I tab the level?
    self.lines.push(new Array(self.lines.level.length).join('  ') + cmd);
  } else {
    // I don't want to not change the format too much...
    self.lines.push('');
  }

  // I need to know "depth."
  // Because I can not tell the difference between a } that
  // closes an object literal and a } that closes a function
  if (cmd) {
    // going down is { and (   e.g. function () {
    // going up is } and )
    var dw = cmd.match(/{|\(/g);
    var up = cmd.match(/}|\)/g);
    up = up ? up.length : 0;
    dw = dw ? dw.length : 0;
    var depth = dw - up;

    if (depth) {
      (function workIt(){
        if (depth > 0) {
          // going... down.
          // push the line#, depth count, and if the line is a function.
          // Since JS only has functional scope I only need to remove
          // "function () {" lines, clearly this will not work for
          // "function ()
          // {" but nothing should break, only tab completion for local
          // scope will not work for this function.
          self.lines.level.push({ line: self.lines.length - 1,
                                depth: depth,
                                isFunction: /\s*function\s*/.test(cmd)});
        } else if (depth < 0) {
          // going... up.
          var curr = self.lines.level.pop();
          if (curr) {
            var tmp = curr.depth + depth;
            if (tmp < 0) {
              //more to go, recurse
              depth += curr.depth;
              workIt();
            } else if (tmp > 0) {
              //remove and push back
              curr.depth += depth;
              self.lines.level.push(curr);
            }
          }
        }
      }());
    }

    // it is possible to determine a syntax error at this point.
    // if the REPL still has a bufferedCommand and
    // self.lines.level.length === 0
    // TODO? keep a log of level so that any syntax breaking lines can
    // be cleared on .break and in the case of a syntax error?
    // TODO? if a log was kept, then I could clear the bufferedComand and
    // eval these lines and throw the syntax error
  } else {
    self.lines.level = [];
  }
};


function defineDefaultCommands(repl) {
  // TODO remove me after 0.3.x
  repl.defineCommand('break', {
    help: 'Sometimes you get stuck, this gets you out',
    action: function() {
      this.bufferedCommand = '';
      this.displayPrompt();
    }
  });

  var clearMessage;
  if (repl.useGlobal) {
    clearMessage = 'Alias for .break';
  } else {
    clearMessage = 'Break, and also clear the local context';
  }
  repl.defineCommand('clear', {
    help: clearMessage,
    action: function() {
      this.bufferedCommand = '';
      if (!this.useGlobal) {
        this.outputStream.write('Clearing context...\n');
        this.resetContext(true);
      }
      this.displayPrompt();
    }
  });

  repl.defineCommand('exit', {
    help: 'Exit the repl',
    action: function() {
      this.rli.close();
    }
  });

  repl.defineCommand('help', {
    help: 'Show repl options',
    action: function() {
      var self = this;
      Object.keys(this.commands).sort().forEach(function(name) {
        var cmd = self.commands[name];
        self.outputStream.write(name + '\t' + (cmd.help || '') + '\n');
      });
      this.displayPrompt();
    }
  });

  repl.defineCommand('save', {
    help: 'Save all evaluated commands in this REPL session to a file',
    action: function(file) {
      try {
        fs.writeFileSync(file, this.lines.join('\n') + '\n');
        this.outputStream.write('Session saved to:' + file + '\n');
      } catch (e) {
        this.outputStream.write('Failed to save:' + file+ '\n')
      }
      this.displayPrompt();
    }
  });

  repl.defineCommand('load', {
    help: 'Load JS from a file into the REPL session',
    action: function(file) {
      try {
        var stats = fs.statSync(file);
        if (stats && stats.isFile()) {
          var self = this;
          var data = fs.readFileSync(file, 'utf8');
          var lines = data.split('\n');
          this.displayPrompt();
          lines.forEach(function (line) {
            if (line) {
              self.rli.write(line + '\n');
            }
          });
        }
      } catch (e) {
        this.outputStream.write('Failed to load:' + file + '\n');
      }
      this.displayPrompt();
    }
  });
}


function trimWhitespace(cmd) {
  var trimmer = /^\s*(.+)\s*$/m,
      matches = trimmer.exec(cmd);

  if (matches && matches.length === 2) {
    return matches[1];
  }
}


function regexpEscape(s) {
  return s.replace(/[-[\]{}()*+?.,\\^$|#\s]/g, '\\$&');
}


/**
 * Converts commands that use var and function <name>() to use the
 * local exports.context when evaled. This provides a local context
 * on the REPL.
 *
 * @param {String} cmd The cmd to convert.
 * @return {String} The converted command.
 */
REPLServer.prototype.convertToContext = function(cmd) {
  var self = this, matches,
      scopeVar = /^\s*var\s*([_\w\$]+)(.*)$/m,
      scopeFunc = /^\s*function\s*([_\w\$]+)/;

  // Replaces: var foo = "bar";  with: self.context.foo = bar;
  matches = scopeVar.exec(cmd);
  if (matches && matches.length === 3) {
    return 'self.context.' + matches[1] + matches[2];
  }

  // Replaces: function foo() {};  with: foo = function foo() {};
  matches = scopeFunc.exec(self.bufferedCommand);
  if (matches && matches.length === 2) {
    return matches[1] + ' = ' + self.bufferedCommand;
  }

  return cmd;
};
