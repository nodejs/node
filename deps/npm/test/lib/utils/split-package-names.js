'use strict'

const t = require('tap')
const splitPackageNames = require('../../../lib/utils/split-package-names.js')

t.test('splitPackageNames', t => {
  const assertions = [
    ['semver', 'semver'],
    ['read-pkg/semver', 'read-pkg/node_modules/semver'],
    ['@npmcli/one/@npmcli/two', '@npmcli/one/node_modules/@npmcli/two'],
    ['@npmcli/one/semver', '@npmcli/one/node_modules/semver'],
  ]

  for (const [input, expected] of assertions)
    t.equal(splitPackageNames(input), expected, `split ${input} correctly`)
  t.end()
})
