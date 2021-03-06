const { resolve, basename } = require('path')
const util = require('util')
const fs = require('fs')
const { unlink } = fs.promises || { unlink: util.promisify(fs.unlink) }
const Arborist = require('@npmcli/arborist')
const log = require('npmlog')

const usageUtil = require('./utils/usage.js')

class Shrinkwrap {
  constructor (npm) {
    this.npm = npm
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  get usage () {
    return usageUtil('shrinkwrap', 'npm shrinkwrap')
  }

  exec (args, cb) {
    this.shrinkwrap().then(() => cb()).catch(cb)
  }

  async shrinkwrap () {
    // if has a npm-shrinkwrap.json, nothing to do
    // if has a package-lock.json, rename to npm-shrinkwrap.json
    // if has neither, load the actual tree and save that as npm-shrinkwrap.json
    // in all cases, re-cast to current lockfile version
    //
    // loadVirtual, fall back to loadActual
    // rename shrinkwrap file type, and tree.meta.save()
    if (this.npm.flatOptions.global) {
      const er = new Error('`npm shrinkwrap` does not work for global packages')
      er.code = 'ESHRINKWRAPGLOBAL'
      throw er
    }

    const path = this.npm.prefix
    const sw = resolve(path, 'npm-shrinkwrap.json')
    const arb = new Arborist({ ...this.npm.flatOptions, path })
    const tree = await arb.loadVirtual().catch(() => arb.loadActual())
    const { meta } = tree
    const newFile = meta.hiddenLockfile || !meta.loadedFromDisk
    const oldFilename = meta.filename
    const notSW = !newFile && basename(oldFilename) !== 'npm-shrinkwrap.json'

    meta.hiddenLockfile = false
    meta.filename = sw
    await meta.save()

    if (newFile)
      log.notice('', 'created a lockfile as npm-shrinkwrap.json')
    else if (notSW) {
      await unlink(oldFilename)
      log.notice('', 'package-lock.json has been renamed to npm-shrinkwrap.json')
    } else if (meta.originalLockfileVersion !== this.npm.lockfileVersion)
      log.notice('', `npm-shrinkwrap.json updated to version ${this.npm.lockfileVersion}`)
    else
      log.notice('', 'npm-shrinkwrap.json up to date')
  }
}
module.exports = Shrinkwrap
