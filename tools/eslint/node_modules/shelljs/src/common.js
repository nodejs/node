// Ignore warning about 'new String()'
/* eslint no-new-wrappers: 0 */
'use strict';

var os = require('os');
var fs = require('fs');
var glob = require('glob');
var shell = require('..');

var shellMethods = Object.create(shell);

// Module globals
var config = {
  silent: false,
  fatal: false,
  verbose: false,
  noglob: false,
  globOptions: {},
  maxdepth: 255
};
exports.config = config;

var state = {
  error: null,
  errorCode: 0,
  currentCmd: 'shell.js',
  tempDir: null
};
exports.state = state;

delete process.env.OLDPWD; // initially, there's no previous directory

var platform = os.type().match(/^Win/) ? 'win' : 'unix';
exports.platform = platform;

// This is populated by calls to commonl.wrap()
var pipeMethods = [];

function log() {
  if (!config.silent) {
    console.error.apply(console, arguments);
  }
}
exports.log = log;

// Shows error message. Throws if config.fatal is true
function error(msg, _code, options) {
  // Validate input
  if (typeof msg !== 'string') throw new Error('msg must be a string');

  var DEFAULT_OPTIONS = {
    continue: false,
    code: 1,
    prefix: state.currentCmd + ': ',
    silent: false,
  };

  if (typeof _code === 'number' && typeof options === 'object') {
    options.code = _code;
  } else if (typeof _code === 'object') { // no 'code'
    options = _code;
  } else if (typeof _code === 'number') { // no 'options'
    options = { code: _code };
  } else if (typeof _code !== 'number') { // only 'msg'
    options = {};
  }
  options = objectAssign({}, DEFAULT_OPTIONS, options);

  if (!state.errorCode) state.errorCode = options.code;

  var logEntry = options.prefix + msg;
  state.error = state.error ? state.error + '\n' : '';
  state.error += logEntry;

  // Throw an error, or log the entry
  if (config.fatal) throw new Error(logEntry);
  if (msg.length > 0 && !options.silent) log(logEntry);

  if (!options.continue) {
    throw {
      msg: 'earlyExit',
      retValue: (new ShellString('', state.error, state.errorCode))
    };
  }
}
exports.error = error;

//@
//@ ### ShellString(str)
//@
//@ Examples:
//@
//@ ```javascript
//@ var foo = ShellString('hello world');
//@ ```
//@
//@ Turns a regular string into a string-like object similar to what each
//@ command returns. This has special methods, like `.to()` and `.toEnd()`
function ShellString(stdout, stderr, code) {
  var that;
  if (stdout instanceof Array) {
    that = stdout;
    that.stdout = stdout.join('\n');
    if (stdout.length > 0) that.stdout += '\n';
  } else {
    that = new String(stdout);
    that.stdout = stdout;
  }
  that.stderr = stderr;
  that.code = code;
  // A list of all commands that can appear on the right-hand side of a pipe
  // (populated by calls to common.wrap())
  pipeMethods.forEach(function (cmd) {
    that[cmd] = shellMethods[cmd].bind(that);
  });
  return that;
}

exports.ShellString = ShellString;

// Return the home directory in a platform-agnostic way, with consideration for
// older versions of node
function getUserHome() {
  var result;
  if (os.homedir) {
    result = os.homedir(); // node 3+
  } else {
    result = process.env[(process.platform === 'win32') ? 'USERPROFILE' : 'HOME'];
  }
  return result;
}
exports.getUserHome = getUserHome;

// Returns {'alice': true, 'bob': false} when passed a string and dictionary as follows:
//   parseOptions('-a', {'a':'alice', 'b':'bob'});
// Returns {'reference': 'string-value', 'bob': false} when passed two dictionaries of the form:
//   parseOptions({'-r': 'string-value'}, {'r':'reference', 'b':'bob'});
function parseOptions(opt, map) {
  if (!map) error('parseOptions() internal error: no map given');

  // All options are false by default
  var options = {};
  Object.keys(map).forEach(function (letter) {
    if (map[letter][0] !== '!') {
      options[map[letter]] = false;
    }
  });

  if (!opt) return options; // defaults

  var optionName;
  if (typeof opt === 'string') {
    if (opt[0] !== '-') {
      return options;
    }

    // e.g. chars = ['R', 'f']
    var chars = opt.slice(1).split('');

    chars.forEach(function (c) {
      if (c in map) {
        optionName = map[c];
        if (optionName[0] === '!') {
          options[optionName.slice(1)] = false;
        } else {
          options[optionName] = true;
        }
      } else {
        error('option not recognized: ' + c);
      }
    });
  } else if (typeof opt === 'object') {
    Object.keys(opt).forEach(function (key) {
      // key is a string of the form '-r', '-d', etc.
      var c = key[1];
      if (c in map) {
        optionName = map[c];
        options[optionName] = opt[key]; // assign the given value
      } else {
        error('option not recognized: ' + c);
      }
    });
  } else {
    error('options must be strings or key-value pairs');
  }
  return options;
}
exports.parseOptions = parseOptions;

// Expands wildcards with matching (ie. existing) file names.
// For example:
//   expand(['file*.js']) = ['file1.js', 'file2.js', ...]
//   (if the files 'file1.js', 'file2.js', etc, exist in the current dir)
function expand(list) {
  if (!Array.isArray(list)) {
    throw new TypeError('must be an array');
  }
  var expanded = [];
  list.forEach(function (listEl) {
    // Don't expand non-strings
    if (typeof listEl !== 'string') {
      expanded.push(listEl);
    } else {
      var ret = glob.sync(listEl, config.globOptions);
      // if glob fails, interpret the string literally
      expanded = expanded.concat(ret.length > 0 ? ret : [listEl]);
    }
  });
  return expanded;
}
exports.expand = expand;

// Normalizes _unlinkSync() across platforms to match Unix behavior, i.e.
// file can be unlinked even if it's read-only, see https://github.com/joyent/node/issues/3006
function unlinkSync(file) {
  try {
    fs.unlinkSync(file);
  } catch (e) {
    // Try to override file permission
    if (e.code === 'EPERM') {
      fs.chmodSync(file, '0666');
      fs.unlinkSync(file);
    } else {
      throw e;
    }
  }
}
exports.unlinkSync = unlinkSync;

// e.g. 'shelljs_a5f185d0443ca...'
function randomFileName() {
  function randomHash(count) {
    if (count === 1) {
      return parseInt(16 * Math.random(), 10).toString(16);
    }
    var hash = '';
    for (var i = 0; i < count; i++) {
      hash += randomHash(1);
    }
    return hash;
  }

  return 'shelljs_' + randomHash(20);
}
exports.randomFileName = randomFileName;

// objectAssign(target_obj, source_obj1 [, source_obj2 ...])
// Ponyfill for Object.assign
//    objectAssign({A:1}, {b:2}, {c:3}) returns {A:1, b:2, c:3}
function objectAssign(target) {
  var sources = [].slice.call(arguments, 1);
  sources.forEach(function (source) {
    Object.keys(source).forEach(function (key) {
      target[key] = source[key];
    });
  });

  return target;
}
exports.extend = Object.assign || objectAssign;

// Common wrapper for all Unix-like commands that performs glob expansion,
// command-logging, and other nice things
function wrap(cmd, fn, options) {
  options = options || {};
  if (options.canReceivePipe) {
    pipeMethods.push(cmd);
  }
  return function () {
    var retValue = null;

    state.currentCmd = cmd;
    state.error = null;
    state.errorCode = 0;

    try {
      var args = [].slice.call(arguments, 0);

      // Log the command to stderr, if appropriate
      if (config.verbose) {
        console.error.apply(console, [cmd].concat(args));
      }

      // If this is coming from a pipe, let's set the pipedValue (otherwise, set
      // it to the empty string)
      state.pipedValue = (this && typeof this.stdout === 'string') ? this.stdout : '';

      if (options.unix === false) { // this branch is for exec()
        retValue = fn.apply(this, args);
      } else { // and this branch is for everything else
        if (args[0] instanceof Object && args[0].constructor.name === 'Object') {
          // a no-op, allowing the syntax `touch({'-r': file}, ...)`
        } else if (args.length === 0 || typeof args[0] !== 'string' || args[0].length <= 1 || args[0][0] !== '-') {
          args.unshift(''); // only add dummy option if '-option' not already present
        }

        // flatten out arrays that are arguments, to make the syntax:
        //    `cp([file1, file2, file3], dest);`
        // equivalent to:
        //    `cp(file1, file2, file3, dest);`
        args = args.reduce(function (accum, cur) {
          if (Array.isArray(cur)) {
            return accum.concat(cur);
          }
          accum.push(cur);
          return accum;
        }, []);

        // Convert ShellStrings (basically just String objects) to regular strings
        args = args.map(function (arg) {
          if (arg instanceof Object && arg.constructor.name === 'String') {
            return arg.toString();
          }
          return arg;
        });

        // Expand the '~' if appropriate
        var homeDir = getUserHome();
        args = args.map(function (arg) {
          if (typeof arg === 'string' && arg.slice(0, 2) === '~/' || arg === '~') {
            return arg.replace(/^~/, homeDir);
          }
          return arg;
        });

        // Perform glob-expansion on all arguments after globStart, but preserve
        // the arguments before it (like regexes for sed and grep)
        if (!config.noglob && options.allowGlobbing === true) {
          args = args.slice(0, options.globStart).concat(expand(args.slice(options.globStart)));
        }

        try {
          // parse options if options are provided
          if (typeof options.cmdOptions === 'object') {
            args[0] = parseOptions(args[0], options.cmdOptions);
          }

          retValue = fn.apply(this, args);
        } catch (e) {
          if (e.msg === 'earlyExit') {
            retValue = e.retValue;
          } else {
            throw e; // this is probably a bug that should be thrown up the call stack
          }
        }
      }
    } catch (e) {
      if (!state.error) {
        // If state.error hasn't been set it's an error thrown by Node, not us - probably a bug...
        console.error('ShellJS: internal error');
        console.error(e.stack || e);
        process.exit(1);
      }
      if (config.fatal) throw e;
    }

    if (options.wrapOutput &&
        (typeof retValue === 'string' || Array.isArray(retValue))) {
      retValue = new ShellString(retValue, state.error, state.errorCode);
    }

    state.currentCmd = 'shell.js';
    return retValue;
  };
} // wrap
exports.wrap = wrap;

// This returns all the input that is piped into the current command (or the
// empty string, if this isn't on the right-hand side of a pipe
function _readFromPipe() {
  return state.pipedValue;
}
exports.readFromPipe = _readFromPipe;

var DEFAULT_WRAP_OPTIONS = {
  allowGlobbing: true,
  canReceivePipe: false,
  cmdOptions: false,
  globStart: 1,
  pipeOnly: false,
  unix: true,
  wrapOutput: true,
  overWrite: false,
};

// Register a new ShellJS command
function _register(name, implementation, wrapOptions) {
  wrapOptions = wrapOptions || {};
  // If an option isn't specified, use the default
  wrapOptions = objectAssign({}, DEFAULT_WRAP_OPTIONS, wrapOptions);

  if (shell[name] && !wrapOptions.overWrite) {
    throw new Error('unable to overwrite `' + name + '` command');
  }

  if (wrapOptions.pipeOnly) {
    wrapOptions.canReceivePipe = true;
    shellMethods[name] = wrap(name, implementation, wrapOptions);
  } else {
    shell[name] = wrap(name, implementation, wrapOptions);
  }
}
exports.register = _register;
