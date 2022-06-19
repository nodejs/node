const isWindows = process.platform === 'win32'
const isWindowsShell = isWindows &&
  !/^MINGW(32|64)$/.test(process.env.MSYSTEM) && process.env.TERM !== 'cygwin'

exports.isWindows = isWindows
exports.isWindowsShell = isWindowsShell
