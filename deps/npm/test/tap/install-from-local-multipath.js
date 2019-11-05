var fs = require('graceful-fs')
var path = require('path')

var mkdirp = require('mkdirp')
var test = require('tap').test

var common = require('../common-tap')

var root = common.pkg
// Allow running this test on older commits (useful for bisecting)
if (!root) {
  var main = require.main.filename
  root = path.resolve(path.dirname(main), path.basename(main, '.js'))
}
var pkg = path.join(root, 'parent')

var EXEC_OPTS = { cwd: pkg }

var parent = {
  name: 'parent',
  version: '0.0.0',
  dependencies: {
    'child-1-1': 'file:../children/child-1-1',
    'child-1-2': 'file:../children/child-1-2',
    'child-2': 'file:../children/child-2'
  }
}

var parentLock = {
  'name': 'parent',
  'version': '1.0.0',
  'lockfileVersion': 1,
  'requires': true,
  'dependencies': {
    'child-1-1': {
      'version': 'file:../children/child-1-1',
      'requires': {
        'child-2': 'file:../children/child-2'
      }
    },
    'child-1-2': {
      'version': 'file:../children/child-1-2',
      'requires': {
        'child-1-1': 'file:../children/child-1-1',
        'child-2': 'file:../children/child-2'
      }
    },
    'child-2': {
      'version': 'file:../children/child-2'
    }
  }
}

var child11 = {
  name: 'parent',
  version: '0.0.0',
  'dependencies': {
    'child-2': 'file:../child-2'
  }
}

var child11Lock = {
  'name': 'child-1-1',
  'version': '1.0.0',
  'lockfileVersion': 1,
  'requires': true,
  'dependencies': {
    'child-2': {
      'version': 'file:../child-2'
    }
  }
}

var child12 = {
  'name': 'child-1-2',
  'version': '1.0.0',
  'dependencies': {
    'child-1-1': 'file:../child-1-1',
    'child-2': 'file:../child-2'
  }
}

var child12Lock = {
  'name': 'child-1-2',
  'version': '1.0.0',
  'lockfileVersion': 1,
  'requires': true,
  'dependencies': {
    'child-1-1': {
      'version': 'file:../child-1-1',
      'requires': {
        'child-2': 'file:../child-2'
      }
    },
    'child-2': {
      'version': 'file:../child-2'
    }
  }
}

var child2 = {
  'name': 'child-2',
  'version': '1.0.0',
  'dependencies': {}
}

var child2Lock = {
  'name': 'child-2',
  'version': '1.0.0',
  'lockfileVersion': 1,
  'requires': true,
  'dependencies': {}
}

test('setup', function (t) {
  mkdirp.sync(pkg)
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(parent, null, 2)
  )

  fs.writeFileSync(
    path.join(pkg, 'package-lock.json'),
    JSON.stringify(parentLock, null, 2)
  )

  mkdirp.sync(path.join(root, 'children', 'child-1-1'))
  fs.writeFileSync(
    path.join(root, 'children', 'child-1-1', 'package.json'),
    JSON.stringify(child11, null, 2)
  )
  fs.writeFileSync(
    path.join(root, 'children', 'child-1-1', 'package-lock.json'),
    JSON.stringify(child11Lock, null, 2)
  )

  mkdirp.sync(path.join(root, 'children', 'child-1-2'))
  fs.writeFileSync(
    path.join(root, 'children', 'child-1-2', 'package.json'),
    JSON.stringify(child12, null, 2)
  )
  fs.writeFileSync(
    path.join(root, 'children', 'child-1-2', 'package-lock.json'),
    JSON.stringify(child12Lock, null, 2)
  )

  mkdirp.sync(path.join(root, 'children', 'child-2'))
  fs.writeFileSync(
    path.join(root, 'children', 'child-2', 'package.json'),
    JSON.stringify(child2, null, 2)
  )
  fs.writeFileSync(
    path.join(root, 'children', 'child-2', 'package-lock.json'),
    JSON.stringify(child2Lock, null, 2)
  )

  process.chdir(pkg)
  t.end()
})

test('\'npm install\' should install local packages', function (t) {
  common.npm(
    [
      'install', '.'
    ],
    EXEC_OPTS,
    function (err, code) {
      t.ifError(err, 'error should not exist')
      t.notOk(code, 'npm install exited with code 0')
      t.end()
    }
  )
})
