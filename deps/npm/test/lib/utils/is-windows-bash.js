const t = require('tap')

const isWindowsBash = () => {
  delete require.cache[require.resolve('../../../lib/utils/is-windows-bash.js')]
  delete require.cache[require.resolve('../../../lib/utils/is-windows.js')]
  return require('../../../lib/utils/is-windows-bash.js')
}

Object.defineProperty(process, 'platform', {
  value: 'posix',
  configurable: true,
})
t.equal(isWindowsBash(), false, 'false when not windows')

Object.defineProperty(process, 'platform', {
  value: 'win32',
  configurable: true,
})
process.env.MSYSTEM = 'not ming'
process.env.TERM = 'dumb'
t.equal(isWindowsBash(), false, 'false when not mingw or cygwin')

process.env.TERM = 'cygwin'
t.equal(isWindowsBash(), true, 'true when cygwin')

process.env.MSYSTEM = 'MINGW64'
process.env.TERM = 'dumb'
t.equal(isWindowsBash(), true, 'true when mingw')
