// this will show what ends up in the fstream-npm package
var P = require('../')
var DW = require('fstream').DirWriter

var target = new DW({ Directory: true, type: 'Directory',
                      path: __dirname + '/output'})

function f (entry) {
  return entry.basename !== '.git'
}

P({ path: './', type: 'Directory', isDirectory: true, filter: f })
  .on('package', function (p) {
    console.error('package', p)
  })
  .on('ignoreFile', function (e) {
    console.error('ignoreFile', e)
  })
  .on('entry', function (e) {
    console.error(e.constructor.name, e.path)
  })
  .pipe(target)
  .on('end', function () {
    console.error('ended')
  })
