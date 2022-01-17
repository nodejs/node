const t = require('tap')
Object.defineProperty(process, 'platform', {
  value: 'win32',
})
const isWindows = require('../../../lib/utils/is-windows.js')
const isWindowsBash = require('../../../lib/utils/is-windows-bash.js')
const isWindowsShell = require('../../../lib/utils/is-windows-shell.js')
t.equal(isWindowsShell, isWindows && !isWindowsBash)
