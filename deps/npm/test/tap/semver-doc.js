var test = require('tap').test

test('semver doc is up to date', function (t) {
  var path = require('path')
  var moddoc = path.join(__dirname, '../../node_modules/semver/README.md')
  var mydoc = path.join(__dirname, '../../docs/content/using-npm/semver.md')
  var fs = require('fs')
  var mod = fs.readFileSync(moddoc, 'utf8')
  mod = mod.substr(mod.match(/^## Install$/m).index)
  var my = fs.readFileSync(mydoc, 'utf8')
  my = my.substr(my.match(/^## Install$/m).index)
  t.equal(my, mod)
  t.end()
})
