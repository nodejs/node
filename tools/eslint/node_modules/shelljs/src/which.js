var common = require('./common');
var fs = require('fs');
var path = require('path');

common.register('which', _which, {
  allowGlobbing: false,
});

// XP's system default value for PATHEXT system variable, just in case it's not
// set on Windows.
var XP_DEFAULT_PATHEXT = '.com;.exe;.bat;.cmd;.vbs;.vbe;.js;.jse;.wsf;.wsh';

// Cross-platform method for splitting environment PATH variables
function splitPath(p) {
  if (!p) return [];

  if (common.platform === 'win') {
    return p.split(';');
  } else {
    return p.split(':');
  }
}

function checkPath(pathName) {
  return fs.existsSync(pathName) && !fs.statSync(pathName).isDirectory();
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
  if (!cmd) common.error('must specify command');

  var pathEnv = process.env.path || process.env.Path || process.env.PATH;
  var pathArray = splitPath(pathEnv);
  var where = null;

  // No relative/absolute paths provided?
  if (cmd.search(/\//) === -1) {
    // Search for command in PATH
    pathArray.forEach(function (dir) {
      if (where) return; // already found it

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
  if (!checkPath(cmd) && !where) return null;

  where = where || path.resolve(cmd);

  return where;
}
module.exports = _which;
