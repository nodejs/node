'use strict'

const common = require('../common-tap.js')
const test = require('tap').test
const fs = require('fs')
const osenv = require('osenv')
const pkg = `${process.env.npm_config_tmp || '/tmp'}/npm-test-publish-config`

require('mkdirp').sync(pkg)

fs.writeFileSync(pkg + '/package.json', JSON.stringify({
  name: 'npm-test-publish-config',
  version: '1.2.3',
  publishConfig: {
    registry: common.registry
  }
}), 'utf8')

fs.writeFileSync(pkg + '/fixture_npmrc',
  '//localhost:' + common.port + '/:email = fancy@feast.net\n' +
  '//localhost:' + common.port + '/:username = fancy\n' +
  '//localhost:' + common.port + '/:_password = ' + Buffer.from('feast').toString('base64'))

test(function (t) {
  let child
  t.plan(5)
  require('http').createServer(function (req, res) {
    t.pass('got request on the fakey fake registry')
    let body = ''
    req.on('data', (d) => { body += d })
    req.on('end', () => {
      this.close()
      res.statusCode = 500
      res.end(JSON.stringify({
        error: 'sshhh. naptime nao. \\^O^/ <(YAWWWWN!)'
      }))
      t.match(body, /"beta"/, 'got expected tag')
      child.kill('SIGINT')
    })
  }).listen(common.port, () => {
    t.pass('server is listening')

    // don't much care about listening to the child's results
    // just wanna make sure it hits the server we just set up.
    //
    // there are plenty of other tests to verify that publish
    // itself functions normally.
    //
    // Make sure that we don't sit around waiting for lock files
    child = common.npm([
      'publish',
      '--userconfig=' + pkg + '/fixture_npmrc',
      '--tag=beta',
      '--loglevel', 'error'
    ], {
      cwd: pkg,
      env: {
        'npm_config_cache_lock_stale': 1000,
        'npm_config_cache_lock_wait': 1000,
        HOME: process.env.HOME,
        Path: process.env.PATH,
        PATH: process.env.PATH,
        USERPROFILE: osenv.home()
      }
    }, function (err, code, stdout, stderr) {
      t.comment(stdout)
      t.comment(stderr)
      t.ifError(err, 'publish command finished successfully')
      t.notOk(code, 'npm install exited with code 0')
    })
  })
})
