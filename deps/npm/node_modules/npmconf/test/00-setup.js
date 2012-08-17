var path = require('path')
var userconfigSrc = path.resolve(__dirname, 'fixtures', 'userconfig')
exports.userconfig = userconfigSrc + '-with-gc'
exports.globalconfig = path.resolve(__dirname, 'fixtures', 'globalconfig')
exports.builtin = path.resolve(__dirname, 'fixtures', 'builtin')

// set the userconfig in the env
// unset anything else that npm might be trying to foist on us
Object.keys(process.env).forEach(function (k) {
  if (k.match(/^npm_config_/i)) {
    delete process.env[k]
  }
})
process.env.npm_config_userconfig = exports.userconfig
process.env.npm_config_other_env_thing = 1000
process.env.random_env_var = 'asdf'

if (module === require.main) {
  // set the globalconfig in the userconfig
  var fs = require('fs')
  var uc = fs.readFileSync(userconfigSrc)
  var gcini = 'globalconfig = ' + exports.globalconfig + '\n'
  fs.writeFileSync(exports.userconfig, gcini + uc)

  console.log('0..1')
  console.log('ok 1 setup done')
}
