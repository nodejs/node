var common = require('../common-tap.js')
var test = require('tap').test
var mkdirp = require('mkdirp')
var fs = require('fs')
var rimraf = require('rimraf')
var path = require('path')

var pkg = common.pkg
var pj = {
  name: 'nested-extraneous',
  version: '1.2.3'
}

var dep = path.resolve(pkg, 'node_modules', 'dep')
var deppj = {
  name: 'nested-extraneous-dep',
  version: '1.2.3',
  dependencies: {
    'nested-extra-depdep': '*'
  }
}

var depdep = path.resolve(dep, 'node_modules', 'depdep')
var depdeppj = {
  name: 'nested-extra-depdep',
  version: '1.2.3'
}

test('setup', function (t) {
  rimraf.sync(pkg)
  mkdirp.sync(depdep)
  fs.writeFileSync(path.resolve(pkg, 'package.json'), JSON.stringify(pj))
  fs.writeFileSync(path.resolve(dep, 'package.json'), JSON.stringify(deppj))
  fs.writeFileSync(path.resolve(depdep, 'package.json'), JSON.stringify(depdeppj))
  t.end()
})

test('test', function (t) {
  common.npm(['ls'], {
    cwd: pkg
  }, function (er, code, sto, ste) {
    if (er) throw er
    t.notEqual(code, 0)
    t.notSimilar(ste, /depdep/)
    t.notSimilar(sto, /depdep/)
    t.end()
  })
})

test('clean', function (t) {
  rimraf.sync(pkg)
  t.end()
})
