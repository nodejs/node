var common = require('../common-tap.js')
var test = require('tap').test
var fs = require('fs')
var osenv = require('osenv')
var pkg = process.env.npm_config_tmp || '/tmp'
pkg += '/npm-test-publish-config'

require('mkdirp').sync(pkg)

fs.writeFileSync(pkg + "/package.json", JSON.stringify({
  name: "npm-test-publish-config",
  version: "1.2.3",
  publishConfig: { registry: common.registry }
}), "utf8")

fs.writeFileSync(pkg + "/fixture_npmrc",
  "//localhost:1337/:email = fancy@feast.net\n" +
  "//localhost:1337/:username = fancy\n" +
  "//localhost:1337/:_password = " + new Buffer("feast").toString("base64") + "\n" +
  "registry = http://localhost:1337/")

var spawn = require('child_process').spawn
var npm = require.resolve('../../bin/npm-cli.js')
var node = process.execPath

test(function (t) {
  var child
  require('http').createServer(function (req, res) {
    t.pass('got request on the fakey fake registry')
    t.end()
    this.close()
    res.statusCode = 500
    res.end('{"error":"sshhh. naptime nao. \\^O^/ <(YAWWWWN!)"}')
    child.kill()
  }).listen(common.port, function () {
    t.pass('server is listening')

    // don't much care about listening to the child's results
    // just wanna make sure it hits the server we just set up.
    //
    // there are plenty of other tests to verify that publish
    // itself functions normally.
    //
    // Make sure that we don't sit around waiting for lock files
    child = spawn(node, [npm, "publish", "--userconfig=" + pkg + "/fixture_npmrc"], {
      cwd: pkg,
      stdio: "inherit",
      env: {
        npm_config_cache_lock_stale: 1000,
        npm_config_cache_lock_wait: 1000,
        HOME: process.env.HOME,
        Path: process.env.PATH,
        PATH: process.env.PATH,
        USERPROFILE: osenv.home()
      }
    })
  })
})
