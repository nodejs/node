// this will show what ends up in the fstream-npm package
var P = require('../')
var tar = require('tar')
function f () {
  return true
}
// function f (entry) {
//   return entry.basename !== ".git"
// }

new P({ path: './', type: 'Directory', isDirectory: true, filter: f })
  .on('package', function (p) {
    console.error('package', p)
  })
  .on('ignoreFile', function (e) {
    console.error('ignoreFile', e)
  })
  .on('entry', function (e) {
    console.error(e.constructor.name, e.path)
  })
  .on('end', function () {
    console.error('ended')
  })
  .pipe(tar.Pack())
  .pipe(process.stdout)
