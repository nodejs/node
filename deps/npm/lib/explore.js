// npm explore <pkg>[@<version>]
// open a subshell to the package folder.

const usageUtil = require('./utils/usage.js')
const completion = require('./utils/completion/installed-shallow.js')
const usage = usageUtil('explore', 'npm explore <pkg> [ -- <command>]')
const rpj = require('read-package-json-fast')

const cmd = (args, cb) => explore(args).then(() => cb()).catch(cb)

const output = require('./utils/output.js')
const npm = require('./npm.js')

const runScript = require('@npmcli/run-script')
const { join, resolve, relative } = require('path')

const explore = async args => {
  if (args.length < 1 || !args[0])
    throw usage

  const pkgname = args.shift()

  // detect and prevent any .. shenanigans
  const path = join(npm.dir, join('/', pkgname))
  if (relative(path, npm.dir) === '')
    throw usage

  // run as if running a script named '_explore', which we set to either
  // the set of arguments, or the shell config, and let @npmcli/run-script
  // handle all the escaping and PATH setup stuff.

  const pkg = await rpj(resolve(path, 'package.json')).catch(er => {
    npm.log.error('explore', `It doesn't look like ${pkgname} is installed.`)
    throw er
  })

  const { shell } = npm.flatOptions
  pkg.scripts = {
    ...(pkg.scripts || {}),
    _explore: args.join(' ').trim() || shell,
  }

  if (!args.length)
    output(`\nExploring ${path}\nType 'exit' or ^D when finished\n`)
  npm.log.disableProgress()
  try {
    return await runScript({
      ...npm.flatOptions,
      pkg,
      banner: false,
      path,
      stdioString: true,
      event: '_explore',
      stdio: 'inherit',
    }).catch(er => {
      process.exitCode = typeof er.code === 'number' && er.code !== 0 ? er.code
        : 1
      // if it's not an exit error, or non-interactive, throw it
      const isProcExit = er.message === 'command failed' &&
        (typeof er.code === 'number' || /^SIG/.test(er.signal || ''))
      if (args.length || !isProcExit)
        throw er
    })
  } finally {
    npm.log.enableProgress()
  }
}

module.exports = Object.assign(cmd, { completion, usage })
