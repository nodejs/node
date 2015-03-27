// this will show what ends up in the fstream-npm package
var P = require('../')
P({ path: './' })
  .on('package', function (p) {
    console.error('package', p)
  })
  .on('ignoreFile', function (e) {
    console.error('ignoreFile', e)
  })
  .on('entry', function (e) {
    console.error(e.constructor.name, e.path.substr(e.root.dirname.length + 1))
  })
