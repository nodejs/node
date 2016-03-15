var fs = require('fs')
var path = require('path')

var tap = require('tap')

var readJson = require('../')

var readme = fs.readFileSync(path.resolve(__dirname, '../README.md'), 'utf8')
var pkg = require('../package.json')
var isGit
try {
  fs.readFileSync(path.resolve(__dirname, '../.git/HEAD'))
  isGit = true
} catch (e) {
  isGit = false
}

tap.test('basic test', function (t) {
  var p = path.resolve(__dirname, '../package.json')
  readJson(p, function (er, data) {
    if (er) throw er
    basic_(t, data)
  })
})

function basic_ (t, data) {
  t.ok(data)
  t.equal(data.version, pkg.version)
  t.equal(data._id, data.name + '@' + data.version)
  t.equal(data.name, pkg.name)
  t.type(data.author, 'object')
  t.equal(data.readme, readme)
  t.deepEqual(data.scripts, pkg.scripts)
  t.equal(data.main, pkg.main)
  t.equal(data.readmeFilename, 'README.md')

  if (isGit) t.similar(data.gitHead, /^[a-f0-9]{40}$/)

  // optional deps are folded in.
  t.deepEqual(data.optionalDependencies, pkg.optionalDependencies)
  t.has(data.dependencies, pkg.optionalDependencies)
  t.has(data.dependencies, pkg.dependencies)

  t.deepEqual(data.devDependencies, pkg.devDependencies)
  t.end()
}
