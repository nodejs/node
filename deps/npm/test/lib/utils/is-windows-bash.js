const t = require('tap')
const mockGlobal = require('../../fixtures/mock-globals.js')

const isWindowsBash = () => {
  delete require.cache[require.resolve('../../../lib/utils/is-windows-bash.js')]
  delete require.cache[require.resolve('../../../lib/utils/is-windows.js')]
  return require('../../../lib/utils/is-windows-bash.js')
}

t.test('posix', (t) => {
  mockGlobal(t, { 'process.platform': 'posix' })
  t.equal(isWindowsBash(), false, 'false when not windows')

  t.end()
})

t.test('win32', (t) => {
  mockGlobal(t, { 'process.platform': 'win32' })

  mockGlobal(t, { 'process.env': { TERM: 'dumb', MSYSTEM: undefined } })
  t.equal(isWindowsBash(), false, 'false when not mingw or cygwin')

  mockGlobal(t, { 'process.env.TERM': 'cygwin' })
  t.equal(isWindowsBash(), true, 'true when cygwin')

  mockGlobal(t, { 'process.env': { TERM: 'dumb', MSYSTEM: 'MINGW64' } })
  t.equal(isWindowsBash(), true, 'true when mingw')

  t.end()
})
