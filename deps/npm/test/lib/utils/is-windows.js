const t = require('tap')
const { isGitBashMsystem } = require('../../../lib/utils/is-windows.js')

t.test('isGitBashMsystem recognizes Git Bash / MSYS2 shells', t => {
  t.equal(isGitBashMsystem('MINGW64'), true)
  t.equal(isGitBashMsystem('MINGW32'), true)
  t.equal(isGitBashMsystem('UCRT64'), true, 'Git for Windows 2.56+ default')
  t.equal(isGitBashMsystem('CLANG64'), true)
  t.equal(isGitBashMsystem('CLANG32'), true)
  t.equal(isGitBashMsystem('CLANGARM64'), true)
  t.equal(isGitBashMsystem('MSYS'), false)
  t.equal(isGitBashMsystem('nogmingw'), false)
  t.equal(isGitBashMsystem(undefined), false)
  t.equal(isGitBashMsystem(''), false)
  t.end()
})

t.test('isWindowsShell respects UCRT64 via fresh require', t => {
  const path = require.resolve('../../../lib/utils/is-windows.js')
  const origPlatform = Object.getOwnPropertyDescriptor(process, 'platform')
  const origMsystem = process.env.MSYSTEM
  const origTerm = process.env.TERM

  const load = (platform, msystem, term) => {
    delete require.cache[path]
    Object.defineProperty(process, 'platform', { value: platform, configurable: true })
    if (msystem === undefined) {
      delete process.env.MSYSTEM
    } else {
      process.env.MSYSTEM = msystem
    }
    if (term === undefined) {
      delete process.env.TERM
    } else {
      process.env.TERM = term
    }
    return require(path).isWindowsShell
  }

  t.teardown(() => {
    delete require.cache[path]
    if (origPlatform) {
      Object.defineProperty(process, 'platform', origPlatform)
    }
    if (origMsystem === undefined) {
      delete process.env.MSYSTEM
    } else {
      process.env.MSYSTEM = origMsystem
    }
    if (origTerm === undefined) {
      delete process.env.TERM
    } else {
      process.env.TERM = origTerm
    }
  })

  t.equal(load('linux', undefined, undefined), false)
  t.equal(load('win32', undefined, undefined), true)
  t.equal(load('win32', 'MINGW64', undefined), false)
  t.equal(load('win32', 'UCRT64', undefined), false, 'UCRT64 Git Bash is not cmd.exe')
  t.equal(load('win32', 'CLANG64', undefined), false)
  t.equal(load('win32', undefined, 'cygwin'), false)
  t.equal(load('win32', 'nogmingw', undefined), true)
  t.end()
})
