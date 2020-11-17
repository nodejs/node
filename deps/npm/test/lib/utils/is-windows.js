const t = require('tap')
const actuallyWindows = process.platform === 'win32'
t.equal(actuallyWindows, require('../../../lib/utils/is-windows.js'))
Object.defineProperty(process, 'platform', {
  value: actuallyWindows ? 'posix' : 'win32',
})
delete require.cache[require.resolve('../../../lib/utils/is-windows.js')]
t.equal(!actuallyWindows, require('../../../lib/utils/is-windows.js'))
