const t = require('tap')
const cliOnlyFlag = require('../../../lib/utils/cli-only-flag.js')
const { patchRelaxOpts } = require('../../../lib/utils/cli-only-flag.js')

// minimal config stub: `where` is the layer find() would resolve the key from
const mockConfig = (where, value) => ({
  find: () => where,
  get: () => value,
})

t.test('returns the value when set on the cli layer', t => {
  t.equal(cliOnlyFlag(mockConfig('cli', true), 'x'), true)
  t.end()
})

t.test('returns undefined when resolved from any non-cli layer', t => {
  for (const where of ['env', 'project', 'user', 'global', 'default']) {
    t.equal(cliOnlyFlag(mockConfig(where, true), 'x'), undefined, `${where} is ignored`)
  }
  t.end()
})

t.test('patchRelaxOpts maps the cli-only patch flags to arborist options', t => {
  const config = {
    find: key => (key === 'allow-unused-patches' ? 'cli' : 'project'),
    get: () => true,
  }
  t.strictSame(patchRelaxOpts(config), {
    allowUnusedPatches: true,
    ignorePatchFailures: undefined,
  })
  t.end()
})
