// Run all the tests in the `npm-registry-couchapp` suite
// This verifies that the server-side stuff still works.

var common = require('../common-tap')
var t = require('tap')

var npmExec = require.resolve('../../bin/npm-cli.js')
var path = require('path')
var ca = path.resolve(__dirname, '../../node_modules/npm-registry-couchapp')

var which = require('which')

var v = process.versions.node.split('.').map(function (n) { return parseInt(n, 10) })
if (v[0] === 0 && v[1] < 10) {
  console.error(
    'WARNING: need a recent Node for npm-registry-couchapp tests to run, have',
    process.versions.node
  )
} else {
  try {
    which.sync('couchdb')
    t.test(runTests)
  } catch (er) {
    console.error('WARNING: need couch to run test: ' + er.message)
  }
}

function runTests (t) {
  var env = Object.assign({ TAP: 1 }, process.env)
  env.npm = npmExec
  // TODO: fix tap and / or nyc to handle nested invocations properly
  env.COVERALLS_REPO_TOKEN = ''

  var opts = {
    cwd: ca,
    stdio: 'inherit'
  }
  common.npm(['install'], opts, function (err, code) {
    if (err) { throw err }
    if (code) {
      t.fail('install failed with: ' + code)
      return t.end()
    } else {
      opts = {
        cwd: ca,
        env: env,
        stdio: 'inherit'
      }
      common.npm(['test', '--', '-Rtap', '--no-coverage'], opts, function (err, code) {
        if (err) { throw err }
        if (code) {
          t.fail('test failed with: ' + code)
          return t.end()
        }
        opts = {
          cwd: ca,
          env: env,
          stdio: 'inherit'
        }
        common.npm(['prune', '--production'], opts, function (err, code) {
          t.ifError(err)
          t.equal(code, 0)
          return t.end()
        })
      })
    }
  })
}
