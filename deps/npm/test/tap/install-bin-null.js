var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var test = require('tap').test

var common = require('../common-tap.js')

var pkg = common.pkg

var EXEC_OPTS = { cwd: pkg }

var parentPkg = {
  name: 'parent-package',
  version: '0.0.0',
  dependencies: {
    'child-package-a': 'file:./child-package-a',
    'child-package-b': 'file:./child-package-b'
  }
}

var childPkgA = {
  name: 'child-package-a',
  version: '0.0.0',
  bin: 'index.js'
}

var childPkgB = {
  name: 'child-package-b',
  version: '0.0.0',
  dependencies: {
    'grandchild-package': 'file:../grandchild-package'
  }
}

var grandchildPkg = {
  name: 'grandchild-package',
  version: '0.0.0',
  bin: null
}

var pkgs = [childPkgA, childPkgB, grandchildPkg]

test('setup', t => {
  mkdirp.sync(pkg)
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(parentPkg, null, 2)
  )
  pkgs.forEach(function (json) {
    var pkgPath = path.resolve(pkg, json.name)
    mkdirp.sync(pkgPath)
    fs.writeFileSync(
      path.join(pkgPath, 'package.json'),
      JSON.stringify(json, null, 2)
    )
  })
  fs.writeFileSync(
    path.join(pkg, childPkgA.name, 'index.js'),
    ''
  )
  t.end()
})

test('the grandchild has bin:null', function (t) {
  common.npm(['install'], EXEC_OPTS, function (err, code, stdout, stderr) {
    t.ifErr(err, 'npm link finished without error')
    t.equal(code, 0, 'exited ok')
    t.ok(stdout, 'output indicating success')
    t.notOk(stderr, 'no output stderr')
    t.end()
  })
})
