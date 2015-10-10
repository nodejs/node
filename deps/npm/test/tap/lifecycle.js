var test = require('tap').test
var npm = require('../../')
var lifecycle = require('../../lib/utils/lifecycle')

test('lifecycle: make env correctly', function (t) {
  npm.load({enteente: Infinity}, function () {
    var env = lifecycle.makeEnv({}, null, process.env)

    t.equal('Infinity', env.npm_config_enteente)
    t.end()
  })
})
