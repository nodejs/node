var common = require('./common');
var fs = require('fs');

//@
//@ ### cat(file [, file ...])
//@ ### cat(file_array)
//@
//@ Examples:
//@
//@ ```javascript
//@ var str = cat('file*.txt');
//@ var str = cat('file1', 'file2');
//@ var str = cat(['file1', 'file2']); // same as above
//@ ```
//@
//@ Returns a string containing the given file, or a concatenated string
//@ containing the files if more than one file is given (a new line character is
//@ introduced between each file). Wildcard `*` accepted.
function _cat(options, files) {
  var cat = '';

  if (!files)
    common.error('no paths given');

  if (typeof files === 'string')
    files = [].slice.call(arguments, 1);
  // if it's array leave it as it is

  files = common.expand(files);

  files.forEach(function(file) {
    if (!fs.existsSync(file))
      common.error('no such file or directory: ' + file);

    cat += fs.readFileSync(file, 'utf8');
  });

  return common.ShellString(cat);
}
module.exports = _cat;

var fs = require('fs');
var common = require('./common');

//@
//@ ### cd([dir])
//@ Changes to directory `dir` for the duration of the script. Changes to home
//@ directory if no argument is supplied.
function _cd(options, dir) {
  if (!dir)
    dir = common.getUserHome();

  if (dir === '-') {
    if (!common.state.previousDir)
      common.error('could not find previous directory');
    else
      dir = common.state.previousDir;
  }

  if (!fs.existsSync(dir))
    common.error('no such file or directory: ' + dir);

  if (!fs.statSync(dir).isDirectory())
    common.error('not a directory: ' + dir);

  common.state.previousDir = process.cwd();
  process.chdir(dir);
}
module.exports = _cd;

var common = require('./common');
var fs = require('fs');
var path = require('path');

var PERMS = (function (base) {
  return {
    OTHER_EXEC  : base.EXEC,
    OTHER_WRITE : base.WRITE,
    OTHER_READ  : base.READ,

    GROUP_EXEC  : base.EXEC  << 3,
    GROUP_WRITE : base.WRITE << 3,
    GROUP_READ  : base.READ << 3,

    OWNER_EXEC  : base.EXEC << 6,
    OWNER_WRITE : base.WRITE << 6,
    OWNER_READ  : base.READ << 6,

    // Literal octal numbers are apparently not allowed in "strict" javascript.  Using parseInt is
    // the preferred way, else a jshint warning is thrown.
    STICKY      : parseInt('01000', 8),
    SETGID      : parseInt('02000', 8),
    SETUID      : parseInt('04000', 8),

    TYPE_MASK   : parseInt('0770000', 8)
  };
})({
  EXEC  : 1,
  WRITE : 2,
  READ  : 4
});

//@
//@ ### chmod(octal_mode || octal_string, file)
//@ ### chmod(symbolic_mode, file)
//@
//@ Available options:
//@
//@ + `-v`: output a diagnostic for every file processed//@
//@ + `-c`: like verbose but report only when a change is made//@
//@ + `-R`: change files and directories recursively//@
//@
//@ Examples:
//@
//@ ```javascript
//@ chmod(755, '/Users/brandon');
//@ chmod('755', '/Users/brandon'); // same as above
//@ chmod('u+x', '/Users/brandon');
//@ ```
//@
//@ Alters the permissions of a file or directory by either specifying the
//@ absolute permissions in octal form or expressing the changes in symbols.
//@ This command tries to mimic the POSIX behavior as much as possible.
//@ Notable exceptions:
//@
//@ + In symbolic modes, 'a-r' and '-r' are identical.  No consideration is
//@   given to the umask.
//@ + There is no "quiet" option since default behavior is to run silent.
function _chmod(options, mode, filePattern) {
  if (!filePattern) {
    if (options.length > 0 && options.charAt(0) === '-') {
      // Special case where the specified file permissions started with - to subtract perms, which
      // get picked up by the option parser as command flags.
      // If we are down by one argument and options starts with -, shift everything over.
      filePattern = mode;
      mode = options;
      options = '';
    }
    else {
      common.error('You must specify a file.');
    }
  }

  options = common.parseOptions(options, {
    'R': 'recursive',
    'c': 'changes',
    'v': 'verbose'
  });

  if (typeof filePattern === 'string') {
    filePattern = [ filePattern ];
  }

  var files;

  if (options.recursive) {
    files = [];
    common.expand(filePattern).forEach(function addFile(expandedFile) {
      var stat = fs.lstatSync(expandedFile);

      if (!stat.isSymbolicLink()) {
        files.push(expandedFile);

        if (stat.isDirectory()) {  // intentionally does not follow symlinks.
          fs.readdirSync(expandedFile).forEach(function (child) {
            addFile(expandedFile + '/' + child);
          });
        }
      }
    });
  }
  else {
    files = common.expand(filePattern);
  }

  files.forEach(function innerChmod(file) {
    file = path.resolve(file);
    if (!fs.existsSync(file)) {
      common.error('File not found: ' + file);
    }

    // When recursing, don't follow symlinks.
    if (options.recursive && fs.lstatSync(file).isSymbolicLink()) {
      return;
    }

    var stat = fs.statSync(file);
    var isDir = stat.isDirectory();
    var perms = stat.mode;
    var type = perms & PERMS.TYPE_MASK;

    var newPerms = perms;

    if (isNaN(parseInt(mode, 8))) {
      // parse options
      mode.split(',').forEach(function (symbolicMode) {
        /*jshint regexdash:true */
        var pattern = /([ugoa]*)([=\+-])([rwxXst]*)/i;
        var matches = pattern.exec(symbolicMode);

        if (matches) {
          var applyTo = matches[1];
          var operator = matches[2];
          var change = matches[3];

          var changeOwner = applyTo.indexOf('u') != -1 || applyTo === 'a' || applyTo === '';
          var changeGroup = applyTo.indexOf('g') != -1 || applyTo === 'a' || applyTo === '';
          var changeOther = applyTo.indexOf('o') != -1 || applyTo === 'a' || applyTo === '';

          var changeRead    = change.indexOf('r') != -1;
          var changeWrite   = change.indexOf('w') != -1;
          var changeExec    = change.indexOf('x') != -1;
          var changeExecDir = change.indexOf('X') != -1;
          var changeSticky  = change.indexOf('t') != -1;
          var changeSetuid  = change.indexOf('s') != -1;

          if (changeExecDir && isDir)
            changeExec = true;

          var mask = 0;
          if (changeOwner) {
            mask |= (changeRead ? PERMS.OWNER_READ : 0) + (changeWrite ? PERMS.OWNER_WRITE : 0) + (changeExec ? PERMS.OWNER_EXEC : 0) + (changeSetuid ? PERMS.SETUID : 0);
          }
          if (changeGroup) {
            mask |= (changeRead ? PERMS.GROUP_READ : 0) + (changeWrite ? PERMS.GROUP_WRITE : 0) + (changeExec ? PERMS.GROUP_EXEC : 0) + (changeSetuid ? PERMS.SETGID : 0);
          }
          if (changeOther) {
            mask |= (changeRead ? PERMS.OTHER_READ : 0) + (changeWrite ? PERMS.OTHER_WRITE : 0) + (changeExec ? PERMS.OTHER_EXEC : 0);
          }

          // Sticky bit is special - it's not tied to user, group or other.
          if (changeSticky) {
            mask |= PERMS.STICKY;
          }

          switch (operator) {
            case '+':
              newPerms |= mask;
              break;

            case '-':
              newPerms &= ~mask;
              break;

            case '=':
              newPerms = type + mask;

              // According to POSIX, when using = to explicitly set the permissions, setuid and setgid can never be cleared.
              if (fs.statSync(file).isDirectory()) {
                newPerms |= (PERMS.SETUID + PERMS.SETGID) & perms;
              }
              break;
          }

          if (options.verbose) {
            console.log(file + ' -> ' + newPerms.toString(8));
          }

          if (perms != newPerms) {
            if (!options.verbose && options.changes) {
              console.log(file + ' -> ' + newPerms.toString(8));
            }
            fs.chmodSync(file, newPerms);
            perms = newPerms; // for the next round of changes!
          }
        }
        else {
          common.error('Invalid symbolic mode change: ' + symbolicMode);
        }
      });
    }
    else {
      // they gave us a full number
      newPerms = type + parseInt(mode, 8);

      // POSIX rules are that setuid and setgid can only be added using numeric form, but not cleared.
      if (fs.statSync(file).isDirectory()) {
        newPerms |= (PERMS.SETUID + PERMS.SETGID) & perms;
      }

      fs.chmodSync(file, newPerms);
    }
  });
}
module.exports = _chmod;

var os = require('os');
var fs = require('fs');
var _ls = require('./ls');

// Module globals
var config = {
  silent: false,
  fatal: false,
  verbose: false,
};
exports.config = config;

var state = {
  error: null,
  currentCmd: 'shell.js',
  previousDir: null,
  tempDir: null
};
exports.state = state;

var platform = os.type().match(/^Win/) ? 'win' : 'unix';
exports.platform = platform;

function log() {
  if (!config.silent)
    console.error.apply(console, arguments);
}
exports.log = log;

// Shows error message. Throws unless _continue or config.fatal are true
function error(msg, _continue) {
  if (state.error === null)
    state.error = '';
  var log_entry = state.currentCmd + ': ' + msg;
  if (state.error === '')
    state.error = log_entry;
  else
    state.error += '\n' + log_entry;

  if (msg.length > 0)
    log(log_entry);

  if (config.fatal)
    process.exit(1);

  if (!_continue)
    throw '';
}
exports.error = error;

// In the future, when Proxies are default, we can add methods like `.to()` to primitive strings.
// For now, this is a dummy function to bookmark places we need such strings
function ShellString(str) {
  return str;
}
exports.ShellString = ShellString;

// Return the home directory in a platform-agnostic way, with consideration for
// older versions of node
function getUserHome() {
  var result;
  if (os.homedir)
    result = os.homedir(); // node 3+
  else
    result = process.env[(process.platform === 'win32') ? 'USERPROFILE' : 'HOME'];
  return result;
}
exports.getUserHome = getUserHome;

// Returns {'alice': true, 'bob': false} when passed a dictionary, e.g.:
//   parseOptions('-a', {'a':'alice', 'b':'bob'});
function parseOptions(str, map) {
  if (!map)
    error('parseOptions() internal error: no map given');

  // All options are false by default
  var options = {};
  for (var letter in map) {
    if (!map[letter].match('^!'))
      options[map[letter]] = false;
  }

  if (!str)
    return options; // defaults

  if (typeof str !== 'string')
    error('parseOptions() internal error: wrong str');

  // e.g. match[1] = 'Rf' for str = '-Rf'
  var match = str.match(/^\-(.+)/);
  if (!match)
    return options;

  // e.g. chars = ['R', 'f']
  var chars = match[1].split('');

  var opt;
  chars.forEach(function(c) {
    if (c in map) {
      opt = map[c];
      if (opt.match('^!'))
        options[opt.slice(1, opt.length-1)] = false;
      else
        options[opt] = true;
    } else {
      error('option not recognized: '+c);
    }
  });

  return options;
}
exports.parseOptions = parseOptions;

// Expands wildcards with matching (ie. existing) file names.
// For example:
//   expand(['file*.js']) = ['file1.js', 'file2.js', ...]
//   (if the files 'file1.js', 'file2.js', etc, exist in the current dir)
function expand(list) {
  var expanded = [];
  list.forEach(function(listEl) {
    // Wildcard present on directory names ?
    if(listEl.search(/\*[^\/]*\//) > -1 || listEl.search(/\*\*[^\/]*\//) > -1) {
      var match = listEl.match(/^([^*]+\/|)(.*)/);
      var root = match[1];
      var rest = match[2];
      var restRegex = rest.replace(/\*\*/g, ".*").replace(/\*/g, "[^\\/]*");
      restRegex = new RegExp(restRegex);

      _ls('-R', root).filter(function (e) {
        return restRegex.test(e);
      }).forEach(function(file) {
        expanded.push(file);
      });
    }
    // Wildcard present on file names ?
    else if (listEl.search(/\*/) > -1) {
      _ls('', listEl).forEach(function(file) {
        expanded.push(file);
      });
    } else {
      expanded.push(listEl);
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
  } catch(e) {
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
    if (count === 1)
      return parseInt(16*Math.random(), 10).toString(16);
    else {
      var hash = '';
      for (var i=0; i<count; i++)
        hash += randomHash(1);
      return hash;
    }
  }

  return 'shelljs_'+randomHash(20);
}
exports.randomFileName = randomFileName;

// extend(target_obj, source_obj1 [, source_obj2 ...])
// Shallow extend, e.g.:
//    extend({A:1}, {b:2}, {c:3}) returns {A:1, b:2, c:3}
function extend(target) {
  var sources = [].slice.call(arguments, 1);
  sources.forEach(function(source) {
    for (var key in source)
      target[key] = source[key];
  });

  return target;
}
exports.extend = extend;

// Common wrapper for all Unix-like commands
function wrap(cmd, fn, options) {
  return function() {
    var retValue = null;

    state.currentCmd = cmd;
    state.error = null;

    try {
      var args = [].slice.call(arguments, 0);

      if (config.verbose) {
        args.unshift(cmd);
        console.log.apply(console, args);
        args.shift();
      }

      if (options && options.notUnix) {
        retValue = fn.apply(this, args);
      } else {
        if (args.length === 0 || typeof args[0] !== 'string' || args[0].length <= 1 || args[0][0] !== '-')
          args.unshift(''); // only add dummy option if '-option' not already present
        // Expand the '~' if appropriate
        var homeDir = getUserHome();
        args = args.map(function(arg) {
          if (typeof arg === 'string' && arg.slice(0, 2) === '~/' || arg === '~')
            return arg.replace(/^~/, homeDir);
          else
            return arg;
        });
        retValue = fn.apply(this, args);
      }
    } catch (e) {
      if (!state.error) {
        // If state.error hasn't been set it's an error thrown by Node, not us - probably a bug...
        console.log('shell.js: internal error');
        console.log(e.stack || e);
        process.exit(1);
      }
      if (config.fatal)
        throw e;
    }

    state.currentCmd = 'shell.js';
    return retValue;
  };
} // wrap
exports.wrap = wrap;

var fs = require('fs');
var path = require('path');
var common = require('./common');
var os = require('os');

// Buffered file copy, synchronous
// (Using readFileSync() + writeFileSync() could easily cause a memory overflow
//  with large files)
function copyFileSync(srcFile, destFile) {
  if (!fs.existsSync(srcFile))
    common.error('copyFileSync: no such file or directory: ' + srcFile);

  var BUF_LENGTH = 64*1024,
      buf = new Buffer(BUF_LENGTH),
      bytesRead = BUF_LENGTH,
      pos = 0,
      fdr = null,
      fdw = null;

  try {
    fdr = fs.openSync(srcFile, 'r');
  } catch(e) {
    common.error('copyFileSync: could not read src file ('+srcFile+')');
  }

  try {
    fdw = fs.openSync(destFile, 'w');
  } catch(e) {
    common.error('copyFileSync: could not write to dest file (code='+e.code+'):'+destFile);
  }

  while (bytesRead === BUF_LENGTH) {
    bytesRead = fs.readSync(fdr, buf, 0, BUF_LENGTH, pos);
    fs.writeSync(fdw, buf, 0, bytesRead);
    pos += bytesRead;
  }

  fs.closeSync(fdr);
  fs.closeSync(fdw);

  fs.chmodSync(destFile, fs.statSync(srcFile).mode);
}

// Recursively copies 'sourceDir' into 'destDir'
// Adapted from https://github.com/ryanmcgrath/wrench-js
//
// Copyright (c) 2010 Ryan McGrath
// Copyright (c) 2012 Artur Adib
//
// Licensed under the MIT License
// http://www.opensource.org/licenses/mit-license.php
function cpdirSyncRecursive(sourceDir, destDir, opts) {
  if (!opts) opts = {};

  /* Create the directory where all our junk is moving to; read the mode of the source directory and mirror it */
  var checkDir = fs.statSync(sourceDir);
  try {
    fs.mkdirSync(destDir, checkDir.mode);
  } catch (e) {
    //if the directory already exists, that's okay
    if (e.code !== 'EEXIST') throw e;
  }

  var files = fs.readdirSync(sourceDir);

  for (var i = 0; i < files.length; i++) {
    var srcFile = sourceDir + "/" + files[i];
    var destFile = destDir + "/" + files[i];
    var srcFileStat = fs.lstatSync(srcFile);

    if (srcFileStat.isDirectory()) {
      /* recursion this thing right on back. */
      cpdirSyncRecursive(srcFile, destFile, opts);
    } else if (srcFileStat.isSymbolicLink()) {
      var symlinkFull = fs.readlinkSync(srcFile);
      fs.symlinkSync(symlinkFull, destFile, os.platform() === "win32" ? "junction" : null);
    } else {
      /* At this point, we've hit a file actually worth copying... so copy it on over. */
      if (fs.existsSync(destFile) && opts.no_force) {
        common.log('skipping existing file: ' + files[i]);
      } else {
        copyFileSync(srcFile, destFile);
      }
    }

  } // for files
} // cpdirSyncRecursive


//@
//@ ### cp([options,] source [, source ...], dest)
//@ ### cp([options,] source_array, dest)
//@ Available options:
//@
//@ + `-f`: force (default behavior)
//@ + `-n`: no-clobber
//@ + `-r, -R`: recursive
//@
//@ Examples:
//@
//@ ```javascript
//@ cp('file1', 'dir1');
//@ cp('-Rf', '/tmp/*', '/usr/local/*', '/home/tmp');
//@ cp('-Rf', ['/tmp/*', '/usr/local/*'], '/home/tmp'); // same as above
//@ ```
//@
//@ Copies files. The wildcard `*` is accepted.
function _cp(options, sources, dest) {
  options = common.parseOptions(options, {
    'f': '!no_force',
    'n': 'no_force',
    'R': 'recursive',
    'r': 'recursive'
  });

  // Get sources, dest
  if (arguments.length < 3) {
    common.error('missing <source> and/or <dest>');
  } else if (arguments.length > 3) {
    sources = [].slice.call(arguments, 1, arguments.length - 1);
    dest = arguments[arguments.length - 1];
  } else if (typeof sources === 'string') {
    sources = [sources];
  } else if ('length' in sources) {
    sources = sources; // no-op for array
  } else {
    common.error('invalid arguments');
  }

  var exists = fs.existsSync(dest),
      stats = exists && fs.statSync(dest);

  // Dest is not existing dir, but multiple sources given
  if ((!exists || !stats.isDirectory()) && sources.length > 1)
    common.error('dest is not a directory (too many sources)');

  // Dest is an existing file, but no -f given
  if (exists && stats.isFile() && options.no_force)
    common.error('dest file already exists: ' + dest);

  if (options.recursive) {
    // Recursive allows the shortcut syntax "sourcedir/" for "sourcedir/*"
    // (see Github issue #15)
    sources.forEach(function(src, i) {
      if (src[src.length - 1] === '/') {
        sources[i] += '*';
      // If src is a directory and dest doesn't exist, 'cp -r src dest' should copy src/* into dest
      } else if (fs.statSync(src).isDirectory() && !exists) {
        sources[i] += '/*';
      }
    });

    // Create dest
    try {
      fs.mkdirSync(dest, parseInt('0777', 8));
    } catch (e) {
      // like Unix's cp, keep going even if we can't create dest dir
    }
  }

  sources = common.expand(sources);

  sources.forEach(function(src) {
    if (!fs.existsSync(src)) {
      common.error('no such file or directory: '+src, true);
      return; // skip file
    }

    // If here, src exists
    if (fs.statSync(src).isDirectory()) {
      if (!options.recursive) {
        // Non-Recursive
        common.log(src + ' is a directory (not copied)');
      } else {
        // Recursive
        // 'cp /a/source dest' should create 'source' in 'dest'
        var newDest = path.join(dest, path.basename(src)),
            checkDir = fs.statSync(src);
        try {
          fs.mkdirSync(newDest, checkDir.mode);
        } catch (e) {
          //if the directory already exists, that's okay
          if (e.code !== 'EEXIST') {
            common.error('dest file no such file or directory: ' + newDest, true);
            throw e;
          }
        }

        cpdirSyncRecursive(src, newDest, {no_force: options.no_force});
      }
      return; // done with dir
    }

    // If here, src is a file

    // When copying to '/path/dir':
    //    thisDest = '/path/dir/file1'
    var thisDest = dest;
    if (fs.existsSync(dest) && fs.statSync(dest).isDirectory())
      thisDest = path.normalize(dest + '/' + path.basename(src));

    if (fs.existsSync(thisDest) && options.no_force) {
      common.error('dest file already exists: ' + thisDest, true);
      return; // skip file
    }

    copyFileSync(src, thisDest);
  }); // forEach(src)
}
module.exports = _cp;

var common = require('./common');
var _cd = require('./cd');
var path = require('path');

// Pushd/popd/dirs internals
var _dirStack = [];

function _isStackIndex(index) {
  return (/^[\-+]\d+$/).test(index);
}

function _parseStackIndex(index) {
  if (_isStackIndex(index)) {
    if (Math.abs(index) < _dirStack.length + 1) { // +1 for pwd
      return (/^-/).test(index) ? Number(index) - 1 : Number(index);
    } else {
      common.error(index + ': directory stack index out of range');
    }
  } else {
    common.error(index + ': invalid number');
  }
}

function _actualDirStack() {
  return [process.cwd()].concat(_dirStack);
}

//@
//@ ### pushd([options,] [dir | '-N' | '+N'])
//@
//@ Available options:
//@
//@ + `-n`: Suppresses the normal change of directory when adding directories to the stack, so that only the stack is manipulated.
//@
//@ Arguments:
//@
//@ + `dir`: Makes the current working directory be the top of the stack, and then executes the equivalent of `cd dir`.
//@ + `+N`: Brings the Nth directory (counting from the left of the list printed by dirs, starting with zero) to the top of the list by rotating the stack.
//@ + `-N`: Brings the Nth directory (counting from the right of the list printed by dirs, starting with zero) to the top of the list by rotating the stack.
//@
//@ Examples:
//@
//@ ```javascript
//@ // process.cwd() === '/usr'
//@ pushd('/etc'); // Returns /etc /usr
//@ pushd('+1');   // Returns /usr /etc
//@ ```
//@
//@ Save the current directory on the top of the directory stack and then cd to `dir`. With no arguments, pushd exchanges the top two directories. Returns an array of paths in the stack.
function _pushd(options, dir) {
  if (_isStackIndex(options)) {
    dir = options;
    options = '';
  }

  options = common.parseOptions(options, {
    'n' : 'no-cd'
  });

  var dirs = _actualDirStack();

  if (dir === '+0') {
    return dirs; // +0 is a noop
  } else if (!dir) {
    if (dirs.length > 1) {
      dirs = dirs.splice(1, 1).concat(dirs);
    } else {
      return common.error('no other directory');
    }
  } else if (_isStackIndex(dir)) {
    var n = _parseStackIndex(dir);
    dirs = dirs.slice(n).concat(dirs.slice(0, n));
  } else {
    if (options['no-cd']) {
      dirs.splice(1, 0, dir);
    } else {
      dirs.unshift(dir);
    }
  }

  if (options['no-cd']) {
    dirs = dirs.slice(1);
  } else {
    dir = path.resolve(dirs.shift());
    _cd('', dir);
  }

  _dirStack = dirs;
  return _dirs('');
}
exports.pushd = _pushd;

//@
//@ ### popd([options,] ['-N' | '+N'])
//@
//@ Available options:
//@
//@ + `-n`: Suppresses the normal change of directory when removing directories from the stack, so that only the stack is manipulated.
//@
//@ Arguments:
//@
//@ + `+N`: Removes the Nth directory (counting from the left of the list printed by dirs), starting with zero.
//@ + `-N`: Removes the Nth directory (counting from the right of the list printed by dirs), starting with zero.
//@
//@ Examples:
//@
//@ ```javascript
//@ echo(process.cwd()); // '/usr'
//@ pushd('/etc');       // '/etc /usr'
//@ echo(process.cwd()); // '/etc'
//@ popd();              // '/usr'
//@ echo(process.cwd()); // '/usr'
//@ ```
//@
//@ When no arguments are given, popd removes the top directory from the stack and performs a cd to the new top directory. The elements are numbered from 0 starting at the first directory listed with dirs; i.e., popd is equivalent to popd +0. Returns an array of paths in the stack.
function _popd(options, index) {
  if (_isStackIndex(options)) {
    index = options;
    options = '';
  }

  options = common.parseOptions(options, {
    'n' : 'no-cd'
  });

  if (!_dirStack.length) {
    return common.error('directory stack empty');
  }

  index = _parseStackIndex(index || '+0');

  if (options['no-cd'] || index > 0 || _dirStack.length + index === 0) {
    index = index > 0 ? index - 1 : index;
    _dirStack.splice(index, 1);
  } else {
    var dir = path.resolve(_dirStack.shift());
    _cd('', dir);
  }

  return _dirs('');
}
exports.popd = _popd;

//@
//@ ### dirs([options | '+N' | '-N'])
//@
//@ Available options:
//@
//@ + `-c`: Clears the directory stack by deleting all of the elements.
//@
//@ Arguments:
//@
//@ + `+N`: Displays the Nth directory (counting from the left of the list printed by dirs when invoked without options), starting with zero.
//@ + `-N`: Displays the Nth directory (counting from the right of the list printed by dirs when invoked without options), starting with zero.
//@
//@ Display the list of currently remembered directories. Returns an array of paths in the stack, or a single path if +N or -N was specified.
//@
//@ See also: pushd, popd
function _dirs(options, index) {
  if (_isStackIndex(options)) {
    index = options;
    options = '';
  }

  options = common.parseOptions(options, {
    'c' : 'clear'
  });

  if (options['clear']) {
    _dirStack = [];
    return _dirStack;
  }

  var stack = _actualDirStack();

  if (index) {
    index = _parseStackIndex(index);

    if (index < 0) {
      index = stack.length + index;
    }

    common.log(stack[index]);
    return stack[index];
  }

  common.log(stack.join(' '));

  return stack;
}
exports.dirs = _dirs;

var common = require('./common');

//@
//@ ### echo(string [, string ...])
//@
//@ Examples:
//@
//@ ```javascript
//@ echo('hello world');
//@ var str = echo('hello world');
//@ ```
//@
//@ Prints string to stdout, and returns string with additional utility methods
//@ like `.to()`.
function _echo() {
  var messages = [].slice.call(arguments, 0);
  console.log.apply(console, messages);
  return common.ShellString(messages.join(' '));
}
module.exports = _echo;

var common = require('./common');

//@
//@ ### error()
//@ Tests if error occurred in the last command. Returns `null` if no error occurred,
//@ otherwise returns string explaining the error
function error() {
  return common.state.error;
}
module.exports = error;

var common = require('./common');
var _tempDir = require('./tempdir');
var _pwd = require('./pwd');
var path = require('path');
var fs = require('fs');
var child = require('child_process');

// Hack to run child_process.exec() synchronously (sync avoids callback hell)
// Uses a custom wait loop that checks for a flag file, created when the child process is done.
// (Can't do a wait loop that checks for internal Node variables/messages as
// Node is single-threaded; callbacks and other internal state changes are done in the
// event loop).
function execSync(cmd, opts) {
  var tempDir = _tempDir();
  var stdoutFile = path.resolve(tempDir+'/'+common.randomFileName()),
      stderrFile = path.resolve(tempDir+'/'+common.randomFileName()),
      codeFile = path.resolve(tempDir+'/'+common.randomFileName()),
      scriptFile = path.resolve(tempDir+'/'+common.randomFileName()),
      sleepFile = path.resolve(tempDir+'/'+common.randomFileName());

  var options = common.extend({
    silent: common.config.silent
  }, opts);

  var previousStdoutContent = '',
      previousStderrContent = '';
  // Echoes stdout and stderr changes from running process, if not silent
  function updateStream(streamFile) {
    if (options.silent || !fs.existsSync(streamFile))
      return;

    var previousStreamContent,
        proc_stream;
    if (streamFile === stdoutFile) {
      previousStreamContent = previousStdoutContent;
      proc_stream = process.stdout;
    } else { // assume stderr
      previousStreamContent = previousStderrContent;
      proc_stream = process.stderr;
    }

    var streamContent = fs.readFileSync(streamFile, 'utf8');
    // No changes since last time?
    if (streamContent.length <= previousStreamContent.length)
      return;

    proc_stream.write(streamContent.substr(previousStreamContent.length));
    previousStreamContent = streamContent;
  }

  function escape(str) {
    return (str+'').replace(/([\\"'])/g, "\\$1").replace(/\0/g, "\\0");
  }

  if (fs.existsSync(scriptFile)) common.unlinkSync(scriptFile);
  if (fs.existsSync(stdoutFile)) common.unlinkSync(stdoutFile);
  if (fs.existsSync(stderrFile)) common.unlinkSync(stderrFile);
  if (fs.existsSync(codeFile)) common.unlinkSync(codeFile);

  var execCommand = '"'+process.execPath+'" '+scriptFile;
  var execOptions = {
    env: process.env,
    cwd: _pwd(),
    maxBuffer: 20*1024*1024
  };

  var script;

  if (typeof child.execSync === 'function') {
    script = [
      "var child = require('child_process')",
      "  , fs = require('fs');",
      "var childProcess = child.exec('"+escape(cmd)+"', {env: process.env, maxBuffer: 20*1024*1024}, function(err) {",
      "  fs.writeFileSync('"+escape(codeFile)+"', err ? err.code.toString() : '0');",
      "});",
      "var stdoutStream = fs.createWriteStream('"+escape(stdoutFile)+"');",
      "var stderrStream = fs.createWriteStream('"+escape(stderrFile)+"');",
      "childProcess.stdout.pipe(stdoutStream, {end: false});",
      "childProcess.stderr.pipe(stderrStream, {end: false});",
      "childProcess.stdout.pipe(process.stdout);",
      "childProcess.stderr.pipe(process.stderr);",
      "var stdoutEnded = false, stderrEnded = false;",
      "function tryClosingStdout(){ if(stdoutEnded){ stdoutStream.end(); } }",
      "function tryClosingStderr(){ if(stderrEnded){ stderrStream.end(); } }",
      "childProcess.stdout.on('end', function(){ stdoutEnded = true; tryClosingStdout(); });",
      "childProcess.stderr.on('end', function(){ stderrEnded = true; tryClosingStderr(); });"
    ].join('\n');

    fs.writeFileSync(scriptFile, script);

    if (options.silent) {
      execOptions.stdio = 'ignore';
    } else {
      execOptions.stdio = [0, 1, 2];
    }

    // Welcome to the future
    child.execSync(execCommand, execOptions);
  } else {
    cmd += ' > '+stdoutFile+' 2> '+stderrFile; // works on both win/unix

    script = [
      "var child = require('child_process')",
      "  , fs = require('fs');",
      "var childProcess = child.exec('"+escape(cmd)+"', {env: process.env, maxBuffer: 20*1024*1024}, function(err) {",
      "  fs.writeFileSync('"+escape(codeFile)+"', err ? err.code.toString() : '0');",
      "});"
    ].join('\n');

    fs.writeFileSync(scriptFile, script);

    child.exec(execCommand, execOptions);

    // The wait loop
    // sleepFile is used as a dummy I/O op to mitigate unnecessary CPU usage
    // (tried many I/O sync ops, writeFileSync() seems to be only one that is effective in reducing
    // CPU usage, though apparently not so much on Windows)
    while (!fs.existsSync(codeFile)) { updateStream(stdoutFile); fs.writeFileSync(sleepFile, 'a'); }
    while (!fs.existsSync(stdoutFile)) { updateStream(stdoutFile); fs.writeFileSync(sleepFile, 'a'); }
    while (!fs.existsSync(stderrFile)) { updateStream(stderrFile); fs.writeFileSync(sleepFile, 'a'); }
  }

  // At this point codeFile exists, but it's not necessarily flushed yet.
  // Keep reading it until it is.
  var code = parseInt('', 10);
  while (isNaN(code)) {
    code = parseInt(fs.readFileSync(codeFile, 'utf8'), 10);
  }

  var stdout = fs.readFileSync(stdoutFile, 'utf8');
  var stderr = fs.readFileSync(stderrFile, 'utf8');

  // No biggie if we can't erase the files now -- they're in a temp dir anyway
  try { common.unlinkSync(scriptFile); } catch(e) {}
  try { common.unlinkSync(stdoutFile); } catch(e) {}
  try { common.unlinkSync(stderrFile); } catch(e) {}
  try { common.unlinkSync(codeFile); } catch(e) {}
  try { common.unlinkSync(sleepFile); } catch(e) {}

  // some shell return codes are defined as errors, per http://tldp.org/LDP/abs/html/exitcodes.html
  if (code === 1 || code === 2 || code >= 126)  {
      common.error('', true); // unix/shell doesn't really give an error message after non-zero exit codes
  }
  // True if successful, false if not
  var obj = {
    code: code,
    output: stdout, // deprecated
    stdout: stdout,
    stderr: stderr
  };
  return obj;
} // execSync()

// Wrapper around exec() to enable echoing output to console in real time
function execAsync(cmd, opts, callback) {
  var stdout = '';
  var stderr = '';

  var options = common.extend({
    silent: common.config.silent
  }, opts);

  var c = child.exec(cmd, {env: process.env, maxBuffer: 20*1024*1024}, function(err) {
    if (callback)
      callback(err ? err.code : 0, stdout, stderr);
  });

  c.stdout.on('data', function(data) {
    stdout += data;
    if (!options.silent)
      process.stdout.write(data);
  });

  c.stderr.on('data', function(data) {
    stderr += data;
    if (!options.silent)
      process.stderr.write(data);
  });

  return c;
}

//@
//@ ### exec(command [, options] [, callback])
//@ Available options (all `false` by default):
//@
//@ + `async`: Asynchronous execution. If a callback is provided, it will be set to
//@   `true`, regardless of the passed value.
//@ + `silent`: Do not echo program output to console.
//@
//@ Examples:
//@
//@ ```javascript
//@ var version = exec('node --version', {silent:true}).stdout;
//@
//@ var child = exec('some_long_running_process', {async:true});
//@ child.stdout.on('data', function(data) {
//@   /* ... do something with data ... */
//@ });
//@
//@ exec('some_long_running_process', function(code, stdout, stderr) {
//@   console.log('Exit code:', code);
//@   console.log('Program output:', stdout);
//@   console.log('Program stderr:', stderr);
//@ });
//@ ```
//@
//@ Executes the given `command` _synchronously_, unless otherwise specified.  When in synchronous
//@ mode returns the object `{ code:..., stdout:... , stderr:... }`, containing the program's
//@ `stdout`, `stderr`, and its exit `code`. Otherwise returns the child process object,
//@ and the `callback` gets the arguments `(code, stdout, stderr)`.
//@
//@ **Note:** For long-lived processes, it's best to run `exec()` asynchronously as
//@ the current synchronous implementation uses a lot of CPU. This should be getting
//@ fixed soon.
function _exec(command, options, callback) {
  if (!command)
    common.error('must specify command');

  // Callback is defined instead of options.
  if (typeof options === 'function') {
    callback = options;
    options = { async: true };
  }

  // Callback is defined with options.
  if (typeof options === 'object' && typeof callback === 'function') {
    options.async = true;
  }

  options = common.extend({
    silent: common.config.silent,
    async: false
  }, options);

  if (options.async)
    return execAsync(command, options, callback);
  else
    return execSync(command, options);
}
module.exports = _exec;

var fs = require('fs');
var common = require('./common');
var _ls = require('./ls');

//@
//@ ### find(path [, path ...])
//@ ### find(path_array)
//@ Examples:
//@
//@ ```javascript
//@ find('src', 'lib');
//@ find(['src', 'lib']); // same as above
//@ find('.').filter(function(file) { return file.match(/\.js$/); });
//@ ```
//@
//@ Returns array of all files (however deep) in the given paths.
//@
//@ The main difference from `ls('-R', path)` is that the resulting file names
//@ include the base directories, e.g. `lib/resources/file1` instead of just `file1`.
function _find(options, paths) {
  if (!paths)
    common.error('no path specified');
  else if (typeof paths === 'object')
    paths = paths; // assume array
  else if (typeof paths === 'string')
    paths = [].slice.call(arguments, 1);

  var list = [];

  function pushFile(file) {
    if (common.platform === 'win')
      file = file.replace(/\\/g, '/');
    list.push(file);
  }

  // why not simply do ls('-R', paths)? because the output wouldn't give the base dirs
  // to get the base dir in the output, we need instead ls('-R', 'dir/*') for every directory

  paths.forEach(function(file) {
    pushFile(file);

    if (fs.statSync(file).isDirectory()) {
      _ls('-RA', file+'/*').forEach(function(subfile) {
        pushFile(subfile);
      });
    }
  });

  return list;
}
module.exports = _find;

var common = require('./common');
var fs = require('fs');

//@
//@ ### grep([options,] regex_filter, file [, file ...])
//@ ### grep([options,] regex_filter, file_array)
//@ Available options:
//@
//@ + `-v`: Inverse the sense of the regex and print the lines not matching the criteria.
//@
//@ Examples:
//@
//@ ```javascript
//@ grep('-v', 'GLOBAL_VARIABLE', '*.js');
//@ grep('GLOBAL_VARIABLE', '*.js');
//@ ```
//@
//@ Reads input string from given files and returns a string containing all lines of the
//@ file that match the given `regex_filter`. Wildcard `*` accepted.
function _grep(options, regex, files) {
  options = common.parseOptions(options, {
    'v': 'inverse'
  });

  if (!files)
    common.error('no paths given');

  if (typeof files === 'string')
    files = [].slice.call(arguments, 2);
  // if it's array leave it as it is

  files = common.expand(files);

  var grep = '';
  files.forEach(function(file) {
    if (!fs.existsSync(file)) {
      common.error('no such file or directory: ' + file, true);
      return;
    }

    var contents = fs.readFileSync(file, 'utf8'),
        lines = contents.split(/\r*\n/);
    lines.forEach(function(line) {
      var matched = line.match(regex);
      if ((options.inverse && !matched) || (!options.inverse && matched))
        grep += line + '\n';
    });
  });

  return common.ShellString(grep);
}
module.exports = _grep;

var fs = require('fs');
var path = require('path');
var common = require('./common');

//@
//@ ### ln([options,] source, dest)
//@ Available options:
//@
//@ + `-s`: symlink
//@ + `-f`: force
//@
//@ Examples:
//@
//@ ```javascript
//@ ln('file', 'newlink');
//@ ln('-sf', 'file', 'existing');
//@ ```
//@
//@ Links source to dest. Use -f to force the link, should dest already exist.
function _ln(options, source, dest) {
  options = common.parseOptions(options, {
    's': 'symlink',
    'f': 'force'
  });

  if (!source || !dest) {
    common.error('Missing <source> and/or <dest>');
  }

  source = String(source);
  var sourcePath = path.normalize(source).replace(RegExp(path.sep + '$'), '');
  var isAbsolute = (path.resolve(source) === sourcePath);
  dest = path.resolve(process.cwd(), String(dest));

  if (fs.existsSync(dest)) {
    if (!options.force) {
      common.error('Destination file exists', true);
    }

    fs.unlinkSync(dest);
  }

  if (options.symlink) {
    var isWindows = common.platform === 'win';
    var linkType = isWindows ? 'file' : null;
    var resolvedSourcePath = isAbsolute ? sourcePath : path.resolve(process.cwd(), path.dirname(dest), source);
    if (!fs.existsSync(resolvedSourcePath)) {
      common.error('Source file does not exist', true);
    } else if (isWindows && fs.statSync(resolvedSourcePath).isDirectory()) {
      linkType =  'junction';
    }

    try {
      fs.symlinkSync(linkType === 'junction' ? resolvedSourcePath: source, dest, linkType);
    } catch (err) {
      common.error(err.message);
    }
  } else {
    if (!fs.existsSync(source)) {
      common.error('Source file does not exist', true);
    }
    try {
      fs.linkSync(source, dest);
    } catch (err) {
      common.error(err.message);
    }
  }
}
module.exports = _ln;

var path = require('path');
var fs = require('fs');
var common = require('./common');
var _cd = require('./cd');
var _pwd = require('./pwd');

//@
//@ ### ls([options,] [path, ...])
//@ ### ls([options,] path_array)
//@ Available options:
//@
//@ + `-R`: recursive
//@ + `-A`: all files (include files beginning with `.`, except for `.` and `..`)
//@ + `-d`: list directories themselves, not their contents
//@ + `-l`: list objects representing each file, each with fields containing `ls
//@         -l` output fields. See
//@         [fs.Stats](https://nodejs.org/api/fs.html#fs_class_fs_stats)
//@         for more info
//@
//@ Examples:
//@
//@ ```javascript
//@ ls('projs/*.js');
//@ ls('-R', '/users/me', '/tmp');
//@ ls('-R', ['/users/me', '/tmp']); // same as above
//@ ls('-l', 'file.txt'); // { name: 'file.txt', mode: 33188, nlink: 1, ...}
//@ ```
//@
//@ Returns array of files in the given path, or in current directory if no path provided.
function _ls(options, paths) {
  options = common.parseOptions(options, {
    'R': 'recursive',
    'A': 'all',
    'a': 'all_deprecated',
    'd': 'directory',
    'l': 'long'
  });

  if (options.all_deprecated) {
    // We won't support the -a option as it's hard to image why it's useful
    // (it includes '.' and '..' in addition to '.*' files)
    // For backwards compatibility we'll dump a deprecated message and proceed as before
    common.log('ls: Option -a is deprecated. Use -A instead');
    options.all = true;
  }

  if (!paths)
    paths = ['.'];
  else if (typeof paths === 'object')
    paths = paths; // assume array
  else if (typeof paths === 'string')
    paths = [].slice.call(arguments, 1);

  var list = [];

  // Conditionally pushes file to list - returns true if pushed, false otherwise
  // (e.g. prevents hidden files to be included unless explicitly told so)
  function pushFile(file, query) {
    var name = file.name || file;
    // hidden file?
    if (path.basename(name)[0] === '.') {
      // not explicitly asking for hidden files?
      if (!options.all && !(path.basename(query)[0] === '.' && path.basename(query).length > 1))
        return false;
    }

    if (common.platform === 'win')
      name = name.replace(/\\/g, '/');

    if (file.name) {
      file.name = name;
    } else {
      file = name;
    }
    list.push(file);
    return true;
  }

  paths.forEach(function(p) {
    if (fs.existsSync(p)) {
      var stats = ls_stat(p);
      // Simple file?
      if (stats.isFile()) {
        if (options.long) {
          pushFile(stats, p);
        } else {
          pushFile(p, p);
        }
        return; // continue
      }

      // Simple dir?
      if (options.directory) {
        pushFile(p, p);
        return;
      } else if (stats.isDirectory()) {
        // Iterate over p contents
        fs.readdirSync(p).forEach(function(file) {
          var orig_file = file;
          if (options.long)
            file = ls_stat(path.join(p, file));
          if (!pushFile(file, p))
            return;

          // Recursive?
          if (options.recursive) {
            var oldDir = _pwd();
            _cd('', p);
            if (fs.statSync(orig_file).isDirectory())
              list = list.concat(_ls('-R'+(options.all?'A':''), orig_file+'/*'));
            _cd('', oldDir);
          }
        });
        return; // continue
      }
    }

    // p does not exist - possible wildcard present

    var basename = path.basename(p);
    var dirname = path.dirname(p);
    // Wildcard present on an existing dir? (e.g. '/tmp/*.js')
    if (basename.search(/\*/) > -1 && fs.existsSync(dirname) && fs.statSync(dirname).isDirectory) {
      // Escape special regular expression chars
      var regexp = basename.replace(/(\^|\$|\(|\)|<|>|\[|\]|\{|\}|\.|\+|\?)/g, '\\$1');
      // Translates wildcard into regex
      regexp = '^' + regexp.replace(/\*/g, '.*') + '$';
      // Iterate over directory contents
      fs.readdirSync(dirname).forEach(function(file) {
        if (file.match(new RegExp(regexp))) {
          var file_path = path.join(dirname,  file);
          file_path = options.long ? ls_stat(file_path) : file_path;
          if (file_path.name)
            file_path.name = path.normalize(file_path.name);
          else
            file_path = path.normalize(file_path);
          if (!pushFile(file_path, basename))
            return;

          // Recursive?
          if (options.recursive) {
            var pp = dirname + '/' + file;
            if (fs.lstatSync(pp).isDirectory())
              list = list.concat(_ls('-R'+(options.all?'A':''), pp+'/*'));
          } // recursive
        } // if file matches
      }); // forEach
      return;
    }

    common.error('no such file or directory: ' + p, true);
  });

  return list;
}
module.exports = _ls;


function ls_stat(path) {
  var stats = fs.statSync(path);
  // Note: this object will contain more information than .toString() returns
  stats.name = path;
  stats.toString = function() {
    // Return a string resembling unix's `ls -l` format
    return [this.mode, this.nlink, this.uid, this.gid, this.size, this.mtime, this.name].join(' ');
  };
  return stats;
}

var common = require('./common');
var fs = require('fs');
var path = require('path');

// Recursively creates 'dir'
function mkdirSyncRecursive(dir) {
  var baseDir = path.dirname(dir);

  // Base dir exists, no recursion necessary
  if (fs.existsSync(baseDir)) {
    fs.mkdirSync(dir, parseInt('0777', 8));
    return;
  }

  // Base dir does not exist, go recursive
  mkdirSyncRecursive(baseDir);

  // Base dir created, can create dir
  fs.mkdirSync(dir, parseInt('0777', 8));
}

//@
//@ ### mkdir([options,] dir [, dir ...])
//@ ### mkdir([options,] dir_array)
//@ Available options:
//@
//@ + `-p`: full path (will create intermediate dirs if necessary)
//@
//@ Examples:
//@
//@ ```javascript
//@ mkdir('-p', '/tmp/a/b/c/d', '/tmp/e/f/g');
//@ mkdir('-p', ['/tmp/a/b/c/d', '/tmp/e/f/g']); // same as above
//@ ```
//@
//@ Creates directories.
function _mkdir(options, dirs) {
  options = common.parseOptions(options, {
    'p': 'fullpath'
  });
  if (!dirs)
    common.error('no paths given');

  if (typeof dirs === 'string')
    dirs = [].slice.call(arguments, 1);
  // if it's array leave it as it is

  dirs.forEach(function(dir) {
    if (fs.existsSync(dir)) {
      if (!options.fullpath)
          common.error('path already exists: ' + dir, true);
      return; // skip dir
    }

    // Base dir does not exist, and no -p option given
    var baseDir = path.dirname(dir);
    if (!fs.existsSync(baseDir) && !options.fullpath) {
      common.error('no such file or directory: ' + baseDir, true);
      return; // skip dir
    }

    if (options.fullpath)
      mkdirSyncRecursive(dir);
    else
      fs.mkdirSync(dir, parseInt('0777', 8));
  });
} // mkdir
module.exports = _mkdir;

var fs = require('fs');
var path = require('path');
var common = require('./common');

//@
//@ ### mv([options ,] source [, source ...], dest')
//@ ### mv([options ,] source_array, dest')
//@ Available options:
//@
//@ + `-f`: force (default behavior)
//@ + `-n`: no-clobber
//@
//@ Examples:
//@
//@ ```javascript
//@ mv('-n', 'file', 'dir/');
//@ mv('file1', 'file2', 'dir/');
//@ mv(['file1', 'file2'], 'dir/'); // same as above
//@ ```
//@
//@ Moves files. The wildcard `*` is accepted.
function _mv(options, sources, dest) {
  options = common.parseOptions(options, {
    'f': '!no_force',
    'n': 'no_force'
  });

  // Get sources, dest
  if (arguments.length < 3) {
    common.error('missing <source> and/or <dest>');
  } else if (arguments.length > 3) {
    sources = [].slice.call(arguments, 1, arguments.length - 1);
    dest = arguments[arguments.length - 1];
  } else if (typeof sources === 'string') {
    sources = [sources];
  } else if ('length' in sources) {
    sources = sources; // no-op for array
  } else {
    common.error('invalid arguments');
  }

  sources = common.expand(sources);

  var exists = fs.existsSync(dest),
      stats = exists && fs.statSync(dest);

  // Dest is not existing dir, but multiple sources given
  if ((!exists || !stats.isDirectory()) && sources.length > 1)
    common.error('dest is not a directory (too many sources)');

  // Dest is an existing file, but no -f given
  if (exists && stats.isFile() && options.no_force)
    common.error('dest file already exists: ' + dest);

  sources.forEach(function(src) {
    if (!fs.existsSync(src)) {
      common.error('no such file or directory: '+src, true);
      return; // skip file
    }

    // If here, src exists

    // When copying to '/path/dir':
    //    thisDest = '/path/dir/file1'
    var thisDest = dest;
    if (fs.existsSync(dest) && fs.statSync(dest).isDirectory())
      thisDest = path.normalize(dest + '/' + path.basename(src));

    if (fs.existsSync(thisDest) && options.no_force) {
      common.error('dest file already exists: ' + thisDest, true);
      return; // skip file
    }

    if (path.resolve(src) === path.dirname(path.resolve(thisDest))) {
      common.error('cannot move to self: '+src, true);
      return; // skip file
    }

    fs.renameSync(src, thisDest);
  }); // forEach(src)
} // mv
module.exports = _mv;

// see dirs.js
// see dirs.js
var path = require('path');
var common = require('./common');

//@
//@ ### pwd()
//@ Returns the current directory.
function _pwd() {
  var pwd = path.resolve(process.cwd());
  return common.ShellString(pwd);
}
module.exports = _pwd;

var common = require('./common');
var fs = require('fs');

// Recursively removes 'dir'
// Adapted from https://github.com/ryanmcgrath/wrench-js
//
// Copyright (c) 2010 Ryan McGrath
// Copyright (c) 2012 Artur Adib
//
// Licensed under the MIT License
// http://www.opensource.org/licenses/mit-license.php
function rmdirSyncRecursive(dir, force) {
  var files;

  files = fs.readdirSync(dir);

  // Loop through and delete everything in the sub-tree after checking it
  for(var i = 0; i < files.length; i++) {
    var file = dir + "/" + files[i],
        currFile = fs.lstatSync(file);

    if(currFile.isDirectory()) { // Recursive function back to the beginning
      rmdirSyncRecursive(file, force);
    }

    else if(currFile.isSymbolicLink()) { // Unlink symlinks
      if (force || isWriteable(file)) {
        try {
          common.unlinkSync(file);
        } catch (e) {
          common.error('could not remove file (code '+e.code+'): ' + file, true);
        }
      }
    }

    else // Assume it's a file - perhaps a try/catch belongs here?
      if (force || isWriteable(file)) {
        try {
          common.unlinkSync(file);
        } catch (e) {
          common.error('could not remove file (code '+e.code+'): ' + file, true);
        }
      }
  }

  // Now that we know everything in the sub-tree has been deleted, we can delete the main directory.
  // Huzzah for the shopkeep.

  var result;
  try {
    // Retry on windows, sometimes it takes a little time before all the files in the directory are gone
    var start = Date.now();
    while (true) {
      try {
        result = fs.rmdirSync(dir);
        if (fs.existsSync(dir)) throw { code: "EAGAIN" };
        break;
      } catch(er) {
        // In addition to error codes, also check if the directory still exists and loop again if true
        if (process.platform === "win32" && (er.code === "ENOTEMPTY" || er.code === "EBUSY" || er.code === "EPERM" || er.code === "EAGAIN")) {
          if (Date.now() - start > 1000) throw er;
        } else if (er.code === "ENOENT") {
          // Directory did not exist, deletion was successful
          break;
        } else {
          throw er;
        }
      }
    }
  } catch(e) {
    common.error('could not remove directory (code '+e.code+'): ' + dir, true);
  }

  return result;
} // rmdirSyncRecursive

// Hack to determine if file has write permissions for current user
// Avoids having to check user, group, etc, but it's probably slow
function isWriteable(file) {
  var writePermission = true;
  try {
    var __fd = fs.openSync(file, 'a');
    fs.closeSync(__fd);
  } catch(e) {
    writePermission = false;
  }

  return writePermission;
}

//@
//@ ### rm([options,] file [, file ...])
//@ ### rm([options,] file_array)
//@ Available options:
//@
//@ + `-f`: force
//@ + `-r, -R`: recursive
//@
//@ Examples:
//@
//@ ```javascript
//@ rm('-rf', '/tmp/*');
//@ rm('some_file.txt', 'another_file.txt');
//@ rm(['some_file.txt', 'another_file.txt']); // same as above
//@ ```
//@
//@ Removes files. The wildcard `*` is accepted.
function _rm(options, files) {
  options = common.parseOptions(options, {
    'f': 'force',
    'r': 'recursive',
    'R': 'recursive'
  });
  if (!files)
    common.error('no paths given');

  if (typeof files === 'string')
    files = [].slice.call(arguments, 1);
  // if it's array leave it as it is

  files = common.expand(files);

  files.forEach(function(file) {
    if (!fs.existsSync(file)) {
      // Path does not exist, no force flag given
      if (!options.force)
        common.error('no such file or directory: '+file, true);

      return; // skip file
    }

    // If here, path exists

    var stats = fs.lstatSync(file);
    if (stats.isFile() || stats.isSymbolicLink()) {

      // Do not check for file writing permissions
      if (options.force) {
        common.unlinkSync(file);
        return;
      }

      if (isWriteable(file))
        common.unlinkSync(file);
      else
        common.error('permission denied: '+file, true);

      return;
    } // simple file

    // Path is an existing directory, but no -r flag given
    if (stats.isDirectory() && !options.recursive) {
      common.error('path is a directory', true);
      return; // skip path
    }

    // Recursively remove existing directory
    if (stats.isDirectory() && options.recursive) {
      rmdirSyncRecursive(file, options.force);
    }
  }); // forEach(file)
} // rm
module.exports = _rm;

var common = require('./common');
var fs = require('fs');

//@
//@ ### sed([options,] search_regex, replacement, file [, file ...])
//@ ### sed([options,] search_regex, replacement, file_array)
//@ Available options:
//@
//@ + `-i`: Replace contents of 'file' in-place. _Note that no backups will be created!_
//@
//@ Examples:
//@
//@ ```javascript
//@ sed('-i', 'PROGRAM_VERSION', 'v0.1.3', 'source.js');
//@ sed(/.*DELETE_THIS_LINE.*\n/, '', 'source.js');
//@ ```
//@
//@ Reads an input string from `files` and performs a JavaScript `replace()` on the input
//@ using the given search regex and replacement string or function. Returns the new string after replacement.
function _sed(options, regex, replacement, files) {
  options = common.parseOptions(options, {
    'i': 'inplace'
  });

  if (typeof replacement === 'string' || typeof replacement === 'function')
    replacement = replacement; // no-op
  else if (typeof replacement === 'number')
    replacement = replacement.toString(); // fallback
  else
    common.error('invalid replacement string');

  // Convert all search strings to RegExp
  if (typeof regex === 'string')
    regex = RegExp(regex);

  if (!files)
    common.error('no files given');

  if (typeof files === 'string')
    files = [].slice.call(arguments, 3);
  // if it's array leave it as it is

  files = common.expand(files);

  var sed = [];
  files.forEach(function(file) {
    if (!fs.existsSync(file)) {
      common.error('no such file or directory: ' + file, true);
      return;
    }

    var result = fs.readFileSync(file, 'utf8').split('\n').map(function (line) {
      return line.replace(regex, replacement);
    }).join('\n');

    sed.push(result);

    if (options.inplace)
      fs.writeFileSync(file, result, 'utf8');
  });

  return common.ShellString(sed.join('\n'));
}
module.exports = _sed;

var common = require('./common');

//@
//@ ### set(options)
//@ Available options:
//@
//@ + `+/-e`: exit upon error (`config.fatal`)
//@ + `+/-v`: verbose: show all commands (`config.verbose`)
//@
//@ Examples:
//@
//@ ```javascript
//@ set('-e'); // exit upon first error
//@ set('+e'); // this undoes a "set('-e')"
//@ ```
//@
//@ Sets global configuration variables
function _set(options) {
  if (!options) {
    var args = [].slice.call(arguments, 0);
    if (args.length < 2)
      common.error('must provide an argument');
    options = args[1];
  }
  var negate = (options[0] === '+');
  if (negate) {
    options = '-' + options.slice(1); // parseOptions needs a '-' prefix
  }
  options = common.parseOptions(options, {
    'e': 'fatal',
    'v': 'verbose'
  });

  var key;
  if (negate) {
    for (key in options)
      options[key] = !options[key];
  }

  for (key in options) {
    // Only change the global config if `negate` is false and the option is true
    // or if `negate` is true and the option is false (aka negate !== option)
    if (negate !== options[key]) {
      common.config[key] = options[key];
    }
  }
  return;
}
module.exports = _set;

var common = require('./common');
var os = require('os');
var fs = require('fs');

// Returns false if 'dir' is not a writeable directory, 'dir' otherwise
function writeableDir(dir) {
  if (!dir || !fs.existsSync(dir))
    return false;

  if (!fs.statSync(dir).isDirectory())
    return false;

  var testFile = dir+'/'+common.randomFileName();
  try {
    fs.writeFileSync(testFile, ' ');
    common.unlinkSync(testFile);
    return dir;
  } catch (e) {
    return false;
  }
}


//@
//@ ### tempdir()
//@
//@ Examples:
//@
//@ ```javascript
//@ var tmp = tempdir(); // "/tmp" for most *nix platforms
//@ ```
//@
//@ Searches and returns string containing a writeable, platform-dependent temporary directory.
//@ Follows Python's [tempfile algorithm](http://docs.python.org/library/tempfile.html#tempfile.tempdir).
function _tempDir() {
  var state = common.state;
  if (state.tempDir)
    return state.tempDir; // from cache

  state.tempDir = writeableDir(os.tmpdir && os.tmpdir()) || // node 0.10+
                  writeableDir(os.tmpDir && os.tmpDir()) || // node 0.8+
                  writeableDir(process.env['TMPDIR']) ||
                  writeableDir(process.env['TEMP']) ||
                  writeableDir(process.env['TMP']) ||
                  writeableDir(process.env['Wimp$ScrapDir']) || // RiscOS
                  writeableDir('C:\\TEMP') || // Windows
                  writeableDir('C:\\TMP') || // Windows
                  writeableDir('\\TEMP') || // Windows
                  writeableDir('\\TMP') || // Windows
                  writeableDir('/tmp') ||
                  writeableDir('/var/tmp') ||
                  writeableDir('/usr/tmp') ||
                  writeableDir('.'); // last resort

  return state.tempDir;
}
module.exports = _tempDir;

var common = require('./common');
var fs = require('fs');

//@
//@ ### test(expression)
//@ Available expression primaries:
//@
//@ + `'-b', 'path'`: true if path is a block device
//@ + `'-c', 'path'`: true if path is a character device
//@ + `'-d', 'path'`: true if path is a directory
//@ + `'-e', 'path'`: true if path exists
//@ + `'-f', 'path'`: true if path is a regular file
//@ + `'-L', 'path'`: true if path is a symbolic link
//@ + `'-p', 'path'`: true if path is a pipe (FIFO)
//@ + `'-S', 'path'`: true if path is a socket
//@
//@ Examples:
//@
//@ ```javascript
//@ if (test('-d', path)) { /* do something with dir */ };
//@ if (!test('-f', path)) continue; // skip if it's a regular file
//@ ```
//@
//@ Evaluates expression using the available primaries and returns corresponding value.
function _test(options, path) {
  if (!path)
    common.error('no path given');

  // hack - only works with unary primaries
  options = common.parseOptions(options, {
    'b': 'block',
    'c': 'character',
    'd': 'directory',
    'e': 'exists',
    'f': 'file',
    'L': 'link',
    'p': 'pipe',
    'S': 'socket'
  });

  var canInterpret = false;
  for (var key in options)
    if (options[key] === true) {
      canInterpret = true;
      break;
    }

  if (!canInterpret)
    common.error('could not interpret expression');

  if (options.link) {
    try {
      return fs.lstatSync(path).isSymbolicLink();
    } catch(e) {
      return false;
    }
  }

  if (!fs.existsSync(path))
    return false;

  if (options.exists)
    return true;

  var stats = fs.statSync(path);

  if (options.block)
    return stats.isBlockDevice();

  if (options.character)
    return stats.isCharacterDevice();

  if (options.directory)
    return stats.isDirectory();

  if (options.file)
    return stats.isFile();

  if (options.pipe)
    return stats.isFIFO();

  if (options.socket)
    return stats.isSocket();
} // test
module.exports = _test;

var common = require('./common');
var fs = require('fs');
var path = require('path');

//@
//@ ### 'string'.to(file)
//@
//@ Examples:
//@
//@ ```javascript
//@ cat('input.txt').to('output.txt');
//@ ```
//@
//@ Analogous to the redirection operator `>` in Unix, but works with JavaScript strings (such as
//@ those returned by `cat`, `grep`, etc). _Like Unix redirections, `to()` will overwrite any existing file!_
function _to(options, file) {
  if (!file)
    common.error('wrong arguments');

  if (!fs.existsSync( path.dirname(file) ))
      common.error('no such file or directory: ' + path.dirname(file));

  try {
    fs.writeFileSync(file, this.toString(), 'utf8');
    return this;
  } catch(e) {
    common.error('could not write to file (code '+e.code+'): '+file, true);
  }
}
module.exports = _to;

var common = require('./common');
var fs = require('fs');
var path = require('path');

//@
//@ ### 'string'.toEnd(file)
//@
//@ Examples:
//@
//@ ```javascript
//@ cat('input.txt').toEnd('output.txt');
//@ ```
//@
//@ Analogous to the redirect-and-append operator `>>` in Unix, but works with JavaScript strings (such as
//@ those returned by `cat`, `grep`, etc).
function _toEnd(options, file) {
  if (!file)
    common.error('wrong arguments');

  if (!fs.existsSync( path.dirname(file) ))
      common.error('no such file or directory: ' + path.dirname(file));

  try {
    fs.appendFileSync(file, this.toString(), 'utf8');
    return this;
  } catch(e) {
    common.error('could not append to file (code '+e.code+'): '+file, true);
  }
}
module.exports = _toEnd;

var common = require('./common');
var fs = require('fs');

//@
//@ ### touch([options,] file)
//@ Available options:
//@
//@ + `-a`: Change only the access time
//@ + `-c`: Do not create any files
//@ + `-m`: Change only the modification time
//@ + `-d DATE`: Parse DATE and use it instead of current time
//@ + `-r FILE`: Use FILE's times instead of current time
//@
//@ Examples:
//@
//@ ```javascript
//@ touch('source.js');
//@ touch('-c', '/path/to/some/dir/source.js');
//@ touch({ '-r': FILE }, '/path/to/some/dir/source.js');
//@ ```
//@
//@ Update the access and modification times of each FILE to the current time.
//@ A FILE argument that does not exist is created empty, unless -c is supplied.
//@ This is a partial implementation of *[touch(1)](http://linux.die.net/man/1/touch)*.
function _touch(opts, files) {
  opts = common.parseOptions(opts, {
    'a': 'atime_only',
    'c': 'no_create',
    'd': 'date',
    'm': 'mtime_only',
    'r': 'reference',
  });

  if (!files) {
    common.error('no paths given');
  }


  if (Array.isArray(files)) {
    files.forEach(function(f) {
      touchFile(opts, f);
    });
  } else if (typeof files === 'string') {
    touchFile(opts, files);
  } else {
    common.error('file arg should be a string file path or an Array of string file paths');
  }

}

function touchFile(opts, file) {
  var stat = tryStatFile(file);

  if (stat && stat.isDirectory()) {
    // don't error just exit
    return;
  }

  // if the file doesn't already exist and the user has specified --no-create then
  // this script is finished
  if (!stat && opts.no_create) {
    return;
  }

  // open the file and then close it. this will create it if it doesn't exist but will
  // not truncate the file
  fs.closeSync(fs.openSync(file, 'a'));

  //
  // Set timestamps
  //

  // setup some defaults
  var now = new Date();
  var mtime = opts.date || now;
  var atime = opts.date || now;

  // use reference file
  if (opts.reference) {
    var refStat = tryStatFile(opts.reference);
    if (!refStat) {
      common.error('failed to get attributess of ' + opts.reference);
    }
    mtime = refStat.mtime;
    atime = refStat.atime;
  } else if (opts.date) {
    mtime = opts.date;
    atime = opts.date;
  }

  if (opts.atime_only && opts.mtime_only) {
    // keep the new values of mtime and atime like GNU
  } else if (opts.atime_only) {
    mtime = stat.mtime;
  } else if (opts.mtime_only) {
    atime = stat.atime;
  }

  fs.utimesSync(file, atime, mtime);
}

module.exports = _touch;

function tryStatFile(filePath) {
  try {
    return fs.statSync(filePath);
  } catch (e) {
    return null;
  }
}

var common = require('./common');
var fs = require('fs');
var path = require('path');

// XP's system default value for PATHEXT system variable, just in case it's not
// set on Windows.
var XP_DEFAULT_PATHEXT = '.com;.exe;.bat;.cmd;.vbs;.vbe;.js;.jse;.wsf;.wsh';

// Cross-platform method for splitting environment PATH variables
function splitPath(p) {
  if (!p)
    return [];

  if (common.platform === 'win')
    return p.split(';');
  else
    return p.split(':');
}

function checkPath(path) {
  return fs.existsSync(path) && !fs.statSync(path).isDirectory();
}

//@
//@ ### which(command)
//@
//@ Examples:
//@
//@ ```javascript
//@ var nodeExec = which('node');
//@ ```
//@
//@ Searches for `command` in the system's PATH. On Windows, this uses the
//@ `PATHEXT` variable to append the extension if it's not already executable.
//@ Returns string containing the absolute path to the command.
function _which(options, cmd) {
  if (!cmd)
    common.error('must specify command');

  var pathEnv = process.env.path || process.env.Path || process.env.PATH,
      pathArray = splitPath(pathEnv),
      where = null;

  // No relative/absolute paths provided?
  if (cmd.search(/\//) === -1) {
    // Search for command in PATH
    pathArray.forEach(function(dir) {
      if (where)
        return; // already found it

      var attempt = path.resolve(dir, cmd);

      if (common.platform === 'win') {
        attempt = attempt.toUpperCase();

        // In case the PATHEXT variable is somehow not set (e.g.
        // child_process.spawn with an empty environment), use the XP default.
        var pathExtEnv = process.env.PATHEXT || XP_DEFAULT_PATHEXT;
        var pathExtArray = splitPath(pathExtEnv.toUpperCase());
        var i;

        // If the extension is already in PATHEXT, just return that.
        for (i = 0; i < pathExtArray.length; i++) {
          var ext = pathExtArray[i];
          if (attempt.slice(-ext.length) === ext && checkPath(attempt)) {
            where = attempt;
            return;
          }
        }

        // Cycle through the PATHEXT variable
        var baseAttempt = attempt;
        for (i = 0; i < pathExtArray.length; i++) {
          attempt = baseAttempt + pathExtArray[i];
          if (checkPath(attempt)) {
            where = attempt;
            return;
          }
        }
      } else {
        // Assume it's Unix-like
        if (checkPath(attempt)) {
          where = attempt;
          return;
        }
      }
    });
  }

  // Command not found anywhere?
  if (!checkPath(cmd) && !where)
    return null;

  where = where || path.resolve(cmd);

  return common.ShellString(where);
}
module.exports = _which;
