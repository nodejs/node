#! /usr/bin/env node

var JSONStream = require('./')

if(!module.parent && process.title !== 'browser') {
  process.stdin
    .pipe(JSONStream.parse(process.argv[2]))
    .pipe(JSONStream.stringify('[', ',\n', ']\n', 2))
    .pipe(process.stdout)
}


