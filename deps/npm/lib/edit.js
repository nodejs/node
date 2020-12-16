// npm edit <pkg>
// open the package folder in the $EDITOR

const { resolve } = require('path')
const fs = require('graceful-fs')
const { spawn } = require('child_process')
const npm = require('./npm.js')
const usageUtil = require('./utils/usage.js')
const splitPackageNames = require('./utils/split-package-names.js')

const usage = usageUtil('edit', 'npm edit <pkg>[/<subpkg>...]')
const completion = require('./utils/completion/installed-shallow.js')

function edit (args, cb) {
  if (args.length !== 1)
    return cb(usage)

  const path = splitPackageNames(args[0])
  const dir = resolve(npm.dir, path)

  fs.lstat(dir, (err) => {
    if (err)
      return cb(err)

    const [bin, ...args] = npm.config.get('editor').split(/\s+/)
    const editor = spawn(bin, [...args, dir], { stdio: 'inherit' })
    editor.on('exit', (code) => {
      if (code)
        return cb(new Error(`editor process exited with code: ${code}`))

      npm.commands.rebuild([dir], cb)
    })
  })
}

module.exports = Object.assign(edit, { completion, usage })
