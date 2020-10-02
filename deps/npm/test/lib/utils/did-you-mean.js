const t = require('tap')
const dym = require('../../../lib/utils/did-you-mean.js')
t.equal(dym('asdfa', ['asdf', 'asfd', 'adfs', 'safd', 'foobarbaz', 'foobar']),
  '\nDid you mean this?\n    asdf')
t.equal(dym('asdfa', ['asdf', 'sdfa', 'foo', 'bar', 'fdsa']),
  '\nDid you mean one of these?\n    asdf\n    sdfa')
t.equal(dym('asdfa', ['install', 'list', 'test']), '')
