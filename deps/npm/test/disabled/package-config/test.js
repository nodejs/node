#!/usr/bin/env node

var env = process.env
var orig = require(process.env.npm_package_name + '/package.json').config
var assert = require('assert')

console.log(
  'Before running this test, do:\n' +
  '  npm config set package-config:foo boo\n' +
  "or else it's about to fail."
)
assert.equal(env.npm_package_config_foo, 'boo', 'foo != boo')
assert.equal(orig.foo, 'bar', 'original foo != bar')
assert.equal(env['npm_config_package-config:foo'], 'boo',
             'package-config:foo != boo')
console.log({
  foo: env.npm_package_config_foo,
  orig_foo: orig.foo,
  'package-config:foo': env['npm_config_package-config:foo']
})
