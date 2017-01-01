#!/usr/bin/env node
/* globals cat, cd, echo, grep, sed, ShellString */
require('../global');

echo('Appending docs to README.md');

cd(__dirname + '/..');

// Extract docs from shell.js
var docs = grep('^//@', 'shell.js');

// Now extract docs from the appropriate src/*.js files
docs = docs.replace(/\/\/@include (.+)/g, function (match, path) {
  var file = path.match('.js$') ? path : path + '.js';
  return grep('^//@', file);
});

// Remove '//@'
docs = docs.replace(/\/\/@ ?/g, '');

// Wipe out the old docs
ShellString(cat('README.md').replace(/## Command reference(.|\n)*\n## Team/, '## Command reference\n## Team')).to('README.md');

// Append new docs to README
sed('-i', /## Command reference/, '## Command reference\n\n' + docs, 'README.md');

echo('All done.');
