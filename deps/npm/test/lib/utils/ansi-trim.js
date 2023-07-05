const t = require('tap')
const ansiTrim = require('../../../lib/utils/ansi-trim.js')

t.test('basic', async t => {
  const chalk = await import('chalk').then(v => v.default)
  t.equal(ansiTrim('foo'), 'foo', 'does nothing if no ansis')
  t.equal(ansiTrim(chalk.red('foo')), 'foo', 'strips out ansis')
})
