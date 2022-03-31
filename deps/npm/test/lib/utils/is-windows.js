const t = require('tap')

const mockGlobals = require('../../fixtures/mock-globals')

t.test('is not windows', async t => {
  mockGlobals(t, { 'process.platform': 'posix' })
  t.match({
    isWindows: false,
    isWindowsShell: false,
  }, t.mock('../../../lib/utils/is-windows.js'))
})

t.test('is windows, shell', async t => {
  mockGlobals(t, {
    'process.platform': 'win32',
    'process.env': {
      MSYSTEM: 'notmingw',
      TERM: 'notcygwin',
    },
  })
  t.match({
    isWindows: true,
    isWindowsShell: true,
  }, t.mock('../../../lib/utils/is-windows.js'))
})

t.test('is windows, not shell', async t => {
  mockGlobals(t, {
    'process.platform': 'win32',
    'process.env': {
      MSYSTEM: 'MINGW32',
      TERM: 'cygwin',
    },
  })
  t.match({
    isWindows: true,
    isWindowsShell: false,
  }, t.mock('../../../lib/utils/is-windows.js'))
})
