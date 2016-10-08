#!/usr/bin/env node
// Export Follow in browser-friendly format.
//
// Copyright 2011 Jason Smith, Jarrett Cruger and contributors
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.

var fs = require('fs')
  , path = require('path')
  ;

if(process.argv[1] === module.filename) {
  var source = process.argv[2]
    , dest   = process.argv[3]
    ;

  if(!source || !dest)
    throw new Error("usage: browser-export.js <source> <target>");

  install(source, dest, function(er) {
    if(er) throw er;
  });
}

function install(filename, target, callback) {
  //console.log('Exporting: ' + filename);
  fs.readFile(filename, null, function(er, content) {
    if(er && er.errno) er = new Error(er.stack); // Make a better stack trace.
    if(er) return callback(er);

    // Strip the shebang.
    content = content.toString('utf8');
    var content_lines = content.split(/\n/);
    content_lines[0] = content_lines[0].replace(/^(#!.*)$/, '// $1');

    // TODO
    // content_lines.join('\n'), maybe new Buffer of that

    //Convert the Node module (CommonJS) to RequireJS.
    var require_re = /\brequire\(['"]([\w\d\-_\/\.]+?)['"]\)/g;

    // No idea why I'm doing this but it's cool.
    var match, dependencies = {};
    while(match = require_re.exec(content))
      dependencies[ match[1] ] = true;
    dependencies = Object.keys(dependencies);
    dependencies.forEach(function(dep) {
      //console.log('  depends: ' + dep);
    })

    // In order to keep the error message line numbers correct, this makes an ugly final file.
    content = [ 'require.def(function(require, exports, module) {'
              //, 'var module = {};'
              //, 'var exports = {};'
              //, 'module.exports = exports;'
              , ''
              , content_lines.join('\n')
              , '; return(module.exports);'
              , '}); // define'
              ].join('');
    //content = new Buffer(content);

    fs.writeFile(target, content, 'utf8', function(er) {
      if(er) return callback(er);
      return callback();
    })
  })
}
