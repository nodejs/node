const t = require('tap')
const deref = require('../../../lib/utils/deref-command.js')

t.equal(deref(null), '')
t.equal(deref(8), '')
t.equal(deref('it'), 'install-test')
t.equal(deref('installTe'), 'install-test')
t.equal(deref('birthday'), 'birthday')
t.equal(deref('birt'), '')
