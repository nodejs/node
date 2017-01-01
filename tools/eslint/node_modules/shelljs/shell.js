//
// ShellJS
// Unix shell commands on top of Node's API
//
// Copyright (c) 2012 Artur Adib
// http://github.com/arturadib/shelljs
//

var common = require('./src/common');

//@
//@ All commands run synchronously, unless otherwise stated.
//@ All commands accept standard bash globbing characters (`*`, `?`, etc.),
//@ compatible with the [node glob module](https://github.com/isaacs/node-glob).
//@
//@ For less-commonly used commands and features, please check out our [wiki
//@ page](https://github.com/shelljs/shelljs/wiki).
//@

// Boilerplate
// -----------
// Copy the code block below here & replace variables with appropiate values
// ```
// //@include ./src/fileName
// var functionName = require('./src/fileName');
// exports.nameOfCommand = common.wrap(nameOfCommand, functionName, {globStart: firstIndexToExpand});
// ```
//
// The //@include includes the docs for that command
//
// firstIndexToExpand should usually be 1 (so, put {globStart: 1})
// Increase this value if the command takes arguments that shouldn't be expanded
// with wildcards, such as with the regexes for sed & grep

//@include ./src/cd
require('./src/cd');

//@include ./src/pwd
require('./src/pwd');

//@include ./src/ls
require('./src/ls');

//@include ./src/find
require('./src/find');

//@include ./src/cp
require('./src/cp');

//@include ./src/rm
require('./src/rm');

//@include ./src/mv
require('./src/mv');

//@include ./src/mkdir
require('./src/mkdir');

//@include ./src/test
require('./src/test');

//@include ./src/cat
require('./src/cat');

//@include ./src/head
require('./src/head');

//@include ./src/tail
require('./src/tail');

//@include ./src/to
require('./src/to');

//@include ./src/toEnd
require('./src/toEnd');

//@include ./src/sed
require('./src/sed');

//@include ./src/sort
require('./src/sort');

//@include ./src/uniq
require('./src/uniq');

//@include ./src/grep
require('./src/grep');

//@include ./src/which
require('./src/which');

//@include ./src/echo
require('./src/echo');

//@include ./src/dirs
require('./src/dirs');

//@include ./src/ln
require('./src/ln');

//@
//@ ### exit(code)
//@ Exits the current process with the given exit code.
exports.exit = process.exit;

//@
//@ ### env['VAR_NAME']
//@ Object containing environment variables (both getter and setter). Shortcut to process.env.
exports.env = process.env;

//@include ./src/exec
require('./src/exec');

//@include ./src/chmod
require('./src/chmod');

//@include ./src/touch
require('./src/touch');

//@include ./src/set
require('./src/set');


//@
//@ ## Non-Unix commands
//@

//@include ./src/tempdir
require('./src/tempdir');

//@include ./src/error

exports.error = require('./src/error');

//@include ./src/common
exports.ShellString = common.ShellString;

//@
//@ ### Pipes
//@
//@ Examples:
//@
//@ ```javascript
//@ grep('foo', 'file1.txt', 'file2.txt').sed(/o/g, 'a').to('output.txt');
//@ echo('files with o\'s in the name:\n' + ls().grep('o'));
//@ cat('test.js').exec('node'); // pipe to exec() call
//@ ```
//@
//@ Commands can send their output to another command in a pipe-like fashion.
//@ `sed`, `grep`, `cat`, `exec`, `to`, and `toEnd` can appear on the right-hand
//@ side of a pipe. Pipes can be chained.

//@
//@ ## Configuration
//@

exports.config = common.config;

//@
//@ ### config.silent
//@
//@ Example:
//@
//@ ```javascript
//@ var sh = require('shelljs');
//@ var silentState = sh.config.silent; // save old silent state
//@ sh.config.silent = true;
//@ /* ... */
//@ sh.config.silent = silentState; // restore old silent state
//@ ```
//@
//@ Suppresses all command output if `true`, except for `echo()` calls.
//@ Default is `false`.

//@
//@ ### config.fatal
//@
//@ Example:
//@
//@ ```javascript
//@ require('shelljs/global');
//@ config.fatal = true; // or set('-e');
//@ cp('this_file_does_not_exist', '/dev/null'); // throws Error here
//@ /* more commands... */
//@ ```
//@
//@ If `true` the script will throw a Javascript error when any shell.js
//@ command encounters an error. Default is `false`. This is analogous to
//@ Bash's `set -e`

//@
//@ ### config.verbose
//@
//@ Example:
//@
//@ ```javascript
//@ config.verbose = true; // or set('-v');
//@ cd('dir/');
//@ ls('subdir/');
//@ ```
//@
//@ Will print each command as follows:
//@
//@ ```
//@ cd dir/
//@ ls subdir/
//@ ```

//@
//@ ### config.globOptions
//@
//@ Example:
//@
//@ ```javascript
//@ config.globOptions = {nodir: true};
//@ ```
//@
//@ Use this value for calls to `glob.sync()` instead of the default options.
