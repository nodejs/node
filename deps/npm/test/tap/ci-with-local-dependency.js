const fs = require('graceful-fs')
const path = require('path')

const mkdirp = require('mkdirp')
const t = require('tap')

const common = require('../common-tap.js')

const pkg = common.pkg + '/package'

const EXEC_OPTS = {
  cwd: pkg,
  stdio: [0, 1, 2],
  env: common.newEnv().extend({
    npm_config_registry: common.registry
  })
}

const localDependencyJson = {
  name: 'local-dependency',
  version: '0.0.0',
  dependencies: {
    'test-package': '0.0.0'
  }
}

const dependentJson = {
  name: 'dependent',
  version: '0.0.0',
  dependencies: {
    'local-dependency': '../local-dependency'
  }
}

const target = path.resolve(pkg, '../local-dependency')
const mr = require('npm-registry-mock')
let server
t.teardown(() => {
  if (server) {
    server.close()
  }
})

t.test('setup', function (t) {
  mkdirp.sync(target)
  fs.writeFileSync(
    path.join(target, 'package.json'),
    JSON.stringify(localDependencyJson, null, 2)
  )
  mkdirp.sync(pkg)
  fs.writeFileSync(
    path.join(pkg, 'package.json'),
    JSON.stringify(dependentJson, null, 2)
  )
  mr({ port: common.port }, (er, s) => {
    if (er) {
      throw er
    }
    server = s
    t.end()
  })
})

t.test('\'npm install\' should install local pkg from sub path', function (t) {
  common.npm(['install', '--loglevel=silent'], EXEC_OPTS, function (err, code) {
    if (err) throw err
    t.equal(code, 0, 'npm install exited with code')
    t.ok(fs.statSync(path.resolve(pkg, 'node_modules/local-dependency/package.json')).isFile(), 'local dependency package.json exists')
    t.ok(fs.statSync(path.resolve(pkg, 'node_modules/local-dependency/node_modules/test-package')).isDirectory(), 'transitive dependency installed')
    t.end()
  })
})

t.test('\'npm ci\' should work', function (t) {
  common.npm(['ci', '--loglevel=silent'], EXEC_OPTS, function (err, code) {
    if (err) throw err
    t.equal(code, 0, 'npm install exited with code')
    t.ok(fs.statSync(path.resolve(pkg, 'node_modules/local-dependency/package.json')).isFile(), 'local dependency package.json exists')
    t.ok(fs.statSync(path.resolve(pkg, 'node_modules/local-dependency/node_modules/test-package')).isDirectory(), 'transitive dependency installed')
    t.end()
  })
})
