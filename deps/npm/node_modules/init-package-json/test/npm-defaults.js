var test = require("tap").test
var rimraf = require("rimraf")
var resolve = require("path").resolve

var npm = require("npm")
var init = require("../")

var EXPECTED = {
  name            : "test",
  version         : "3.1.4",
  description     : "",
  main            : "basic.js",
  scripts         : {
    test          : 'echo "Error: no test specified" && exit 1'
  },
  keywords        : [],
  author          : "npmbot <n@p.m> (http://npm.im)",
  license         : "WTFPL"
}

test("npm configuration values pulled from environment", function (t) {
  /*eslint camelcase:0 */
  process.env.npm_config_yes = "yes"

  process.env.npm_config_init_author_name  = "npmbot"
  process.env.npm_config_init_author_email = "n@p.m"
  process.env.npm_config_init_author_url   = "http://npm.im"

  process.env.npm_config_init_license = EXPECTED.license
  process.env.npm_config_init_version = EXPECTED.version

  npm.load({}, function (err) {
    t.ifError(err, "npm loaded successfully")

    process.chdir(resolve(__dirname))
    init(__dirname, __dirname, npm.config, function (er, data) {
      t.ifError(err, "init ran successfully")

      t.same(data, EXPECTED, "got the package data from the environment")
      t.end()
    })
  })
})

test("cleanup", function (t) {
  rimraf.sync(resolve(__dirname, "package.json"))
  t.pass("cleaned up")
  t.end()
})
