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

var disableColors = true;
if (process.platform != 'win32') {
  disableColors = process.env.NODE_DISABLE_COLORS ? true : false;
}


// hack for require.resolve("./relative") to work properly.
module.filename = process.cwd() + '/repl';

// hack for repl require to work properly with node_modules folders
module.paths = require('module')._nodeModulePaths(module.filename);


function resetContext() {
  context = vm.createContext();
  for (var i in global) context[i] = global[i];
  context.module = module;
  context.require = require;
  context.global = context;
  context.global.global = context;
  for (var i in require.cache) delete require.cache[i];
}


// Can overridden with custom print functions, such as `probe` or `eyes.js`
exports.writer = util.inspect;


function REPLServer(prompt, stream) {
  var self = this;
  if (!context) resetContext();
  if (!exports.repl) exports.repl = this;
  self.context = context;
  self.bufferedCommand = '';

  if (stream) {
    // We're given a duplex socket
    self.outputStream = stream;
    self.inputStream = stream;
  } else {
    self.outputStream = process.stdout;
    self.inputStream = process.stdin;
    process.stdin.resume();
  }

  self.prompt = (prompt != undefined ? prompt : '> ');

  function complete(text) {
    return self.complete(text);
  }

  var rli = rl.createInterface(self.inputStream, self.outputStream, complete);
  self.rli = rli;

  this.commands = {};
  defineDefaultCommands(this);

  if (rli.enabled && !disableColors && exports.writer === util.inspect) {
    // Turn on ANSI coloring.
    exports.writer = function(obj, showHidden, depth) {
      return util.inspect(obj, showHidden, depth, true);
    };
  }

  rli.setPrompt(self.prompt);

  rli.on('SIGINT', function() {
    if (self.bufferedCommand && self.bufferedCommand.length > 0) {
      rli.write('\n');
      self.bufferedCommand = '';
      self.displayPrompt();
    } else {
      rli.close();
    }
  });

  rli.addListener('line', function(cmd) {
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
      // The catchall for errors
      try {
        self.bufferedCommand += cmd + '\n';
        // This try is for determining if the command is complete, or should
        // continue onto the next line.
        try {
          // We try to evaluate both expressions e.g.
          //  '{ a : 1 }'
          // and statements e.g.
          //  'for (var i = 0; i < 10; i++) console.log(i);'

          var ret, success = false;
          try {
            // First we attempt to eval as expression with parens.
            // This catches '{a : 1}' properly.
            ret = vm.runInContext('(' + self.bufferedCommand + ')',
                                  context,
                                  'repl');
            if (typeof ret !== 'function') success = true;
          } catch (e) {
            success = false;
          }

          if (!success) {
            // Now as statement without parens.
            ret = vm.runInContext(self.bufferedCommand, context, 'repl');
          }

          if (ret !== undefined) {
            context._ = ret;
            self.outputStream.write(exports.writer(ret) + '\n');
          }

          self.bufferedCommand = '';
        } catch (e) {
          // instanceof doesn't work across context switches.
          if (!(e && e.constructor && e.constructor.name === 'SyntaxError')) {
            throw e;
          // It could also be an error from JSON.parse
          } else if (e &&
                     e.stack &&
                     e.stack.match(/^SyntaxError: Unexpected token .*\n/) &&
                     e.stack.match(/\n    at Object.parse \(native\)\n/)) {
            throw e;
          }
        }
      } catch (e) {
        // On error: Print the error and clear the buffer
        if (e.stack) {
          self.outputStream.write(e.stack + '\n');
        } else {
          self.outputStream.write(e.toString() + '\n');
        }
        self.bufferedCommand = '';
      }
    }

    self.displayPrompt();
  });

  rli.addListener('close', function() {
    self.inputStream.destroy();
  });

  self.displayPrompt();
}
exports.REPLServer = REPLServer;


// prompt is a string to print on each line for the prompt,
// source is a stream to use for I/O, defaulting to stdin/stdout.
exports.start = function(prompt, source) {
  return new REPLServer(prompt, source);
};


REPLServer.prototype.displayPrompt = function() {
  this.rli.setPrompt(this.bufferedCommand.length ? '... ' : this.prompt);
  this.rli.prompt();
};


// read a line from the stream, then eval it
REPLServer.prototype.readline = function(cmd) {
};


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
REPLServer.prototype.complete = function(line) {
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
    for (i = 0; i < require.paths.length; i++) {
      dir = path.resolve(require.paths[i], subdir);
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
        completionGroups.push(Object.getOwnPropertyNames(this.context));
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
            'continue', 'debugger', 'default', 'delete', 'do', 'else', 'export',
            'false', 'finally', 'for', 'function', 'if', 'import', 'in',
            'instanceof', 'let', 'new', 'null', 'return', 'switch', 'this',
            'throw', 'true', 'try', 'typeof', 'undefined', 'var', 'void',
            'while', 'with', 'yield']);
        }
      } else {
        try {
          obj = vm.runInContext(expr, this.context, 'repl');
        } catch (e) {
          //console.log("completion eval error, expr='"+expr+"': "+e);
        }
        if (obj != null) {
          if (typeof obj === 'object' || typeof obj === 'function') {
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
        if (!hasOwnProperty(uniq, c)) {
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

  return [completions || [], completeOn];
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


function defineDefaultCommands(repl) {
  // TODO remove me after 0.3.x
  repl.defineCommand('break', {
    help: 'Sometimes you get stuck, this gets you out',
    action: function() {
      this.bufferedCommand = '';
      this.displayPrompt();
    }
  });

  repl.defineCommand('clear', {
    help: 'Break, and also clear the local context',
    action: function() {
      this.outputStream.write('Clearing context...\n');
      this.bufferedCommand = '';
      resetContext();
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
