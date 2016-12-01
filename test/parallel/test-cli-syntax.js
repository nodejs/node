'use strict';

const common = require('../common');
const assert = require('assert');
const spawnSync = require('child_process').spawnSync;
const path = require('path');

var node = process.execPath;

// test both sets of arguments that check syntax
var syntaxArgs = [
  ['-c'],
  ['--check']
];

// test good syntax with and without shebang
[
  'syntax/good_syntax.js',
  'syntax/good_syntax',
  'syntax/good_syntax_shebang.js',
  'syntax/good_syntax_shebang',
  'syntax/illegal_if_not_wrapped.js'
].forEach(function(file) {
  file = path.join(common.fixturesDir, file);

  // loop each possible option, `-c` or `--check`
  syntaxArgs.forEach(function(args) {
    var _args = args.concat(file);
    var c = spawnSync(node, _args, {encoding: 'utf8'});

    // no output should be produced
    assert.strictEqual(c.stdout, '', 'stdout produced');
    assert.strictEqual(c.stderr, '', 'stderr produced');
    assert.strictEqual(c.status, 0, 'code == ' + c.status);
  });
});

// test bad syntax with and without shebang
[
  'syntax/bad_syntax.js',
  'syntax/bad_syntax',
  'syntax/bad_syntax_shebang.js',
  'syntax/bad_syntax_shebang'
].forEach(function(file) {
  file = path.join(common.fixturesDir, file);

  // loop each possible option, `-c` or `--check`
  syntaxArgs.forEach(function(args) {
    var _args = args.concat(file);
    var c = spawnSync(node, _args, {encoding: 'utf8'});

    // no stdout should be produced
    assert.strictEqual(c.stdout, '', 'stdout produced');

    // stderr should have a syntax error message
    var match = c.stderr.match(/^SyntaxError: Unexpected identifier$/m);
    assert(match, 'stderr incorrect');

    assert.strictEqual(c.status, 1, 'code == ' + c.status);
  });
});

// test file not found
[
  'syntax/file_not_found.js',
  'syntax/file_not_found'
].forEach(function(file) {
  file = path.join(common.fixturesDir, file);

  // loop each possible option, `-c` or `--check`
  syntaxArgs.forEach(function(args) {
    var _args = args.concat(file);
    var c = spawnSync(node, _args, {encoding: 'utf8'});

    // no stdout should be produced
    assert.strictEqual(c.stdout, '', 'stdout produced');

    // stderr should have a module not found error message
    var match = c.stderr.match(/^Error: Cannot find module/m);
    assert(match, 'stderr incorrect');

    assert.strictEqual(c.status, 1, 'code == ' + c.status);
  });
});
