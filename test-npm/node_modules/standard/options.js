var eslint = require('eslint')
var extend = require('xtend')
var pkg = require('./package.json')

var configStandard = require('eslint-config-standard')
var configStandardJsx = require('eslint-config-standard-jsx')

var config = extend(configStandard)
config.plugins.push.apply(config.plugins, configStandardJsx.plugins)

Object.keys(configStandardJsx.rules).forEach(function (key) {
  config.rules[key] = configStandardJsx.rules[key]
})

module.exports = {
  eslint: eslint,
  cmd: 'standard',
  version: pkg.version,
  homepage: pkg.homepage,
  bugs: pkg.bugs.url,
  tagline: 'Use JavaScript Standard Style',
  eslintConfig: {
    baseConfig: config
  },
  formatter: 'Formatting is no longer included with standard. Install it separately: "npm install -g standard-format"'
}
