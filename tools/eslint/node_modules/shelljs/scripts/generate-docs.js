#!/usr/bin/env node
/* globals cat, cd, echo, grep, sed */
require('../global');

echo('Appending docs to README.md');

cd(__dirname + '/..');

// Extract docs from shell.js
var docs = grep('//@', 'shell.js');

docs = docs.replace(/\/\/\@include (.+)/g, function(match, path) {
  var file = path.match('.js$') ? path : path+'.js';
  return grep('//@', file);
});

// Remove '//@'
docs = docs.replace(/\/\/\@ ?/g, '');

// Wipe out the old docs
cat('README.md').replace(/## Command reference(.|\n)*/, '## Command reference').to('README.md');

// Append new docs to README
sed('-i', /## Command reference/, '## Command reference\n\n' + docs, 'README.md');

echo('All done.');
