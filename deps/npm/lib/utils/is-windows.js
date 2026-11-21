const isGitBashMsystem = (msystem) =>
  /^(MINGW(32|64)|UCRT64|CLANG(32|64|ARM64))$/.test(msystem || '')

const isWindowsShell = (process.platform === 'win32') &&
  !isGitBashMsystem(process.env.MSYSTEM) && process.env.TERM !== 'cygwin'

exports.isWindowsShell = isWindowsShell
exports.isGitBashMsystem = isGitBashMsystem
