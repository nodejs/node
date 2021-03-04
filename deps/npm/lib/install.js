/* eslint-disable camelcase */
/* eslint-disable standard/no-callback-literal */
const fs = require('fs')
const util = require('util')
const readdir = util.promisify(fs.readdir)
const usageUtil = require('./utils/usage.js')
const reifyFinish = require('./utils/reify-finish.js')
const log = require('npmlog')
const { resolve, join } = require('path')
const Arborist = require('@npmcli/arborist')
const runScript = require('@npmcli/run-script')

class Install {
  constructor (npm) {
    this.npm = npm
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  get usage () {
    return usageUtil(
      'install',
      'npm install (with no args, in package dir)' +
      '\nnpm install [<@scope>/]<pkg>' +
      '\nnpm install [<@scope>/]<pkg>@<tag>' +
      '\nnpm install [<@scope>/]<pkg>@<version>' +
      '\nnpm install [<@scope>/]<pkg>@<version range>' +
      '\nnpm install <alias>@npm:<name>' +
      '\nnpm install <folder>' +
      '\nnpm install <tarball file>' +
      '\nnpm install <tarball url>' +
      '\nnpm install <git:// url>' +
      '\nnpm install <github username>/<github project>',
      '[--save-prod|--save-dev|--save-optional|--save-peer] [--save-exact] [--no-save]'
    )
  }

  async completion (opts) {
    const { partialWord } = opts
    // install can complete to a folder with a package.json, or any package.
    // if it has a slash, then it's gotta be a folder
    // if it starts with https?://, then just give up, because it's a url
    if (/^https?:\/\//.test(partialWord)) {
      // do not complete to URLs
      return []
    }

    if (/\//.test(partialWord)) {
      // Complete fully to folder if there is exactly one match and it
      // is a folder containing a package.json file.  If that is not the
      // case we return 0 matches, which will trigger the default bash
      // complete.
      const lastSlashIdx = partialWord.lastIndexOf('/')
      const partialName = partialWord.slice(lastSlashIdx + 1)
      const partialPath = partialWord.slice(0, lastSlashIdx) || '/'

      const annotatePackageDirMatch = async (sibling) => {
        const fullPath = join(partialPath, sibling)
        if (sibling.slice(0, partialName.length) !== partialName)
          return null // not name match

        try {
          const contents = await readdir(fullPath)
          return {
            fullPath,
            isPackage: contents.indexOf('package.json') !== -1,
          }
        } catch (er) {
          return { isPackage: false }
        }
      }

      try {
        const siblings = await readdir(partialPath)
        const matches = await Promise.all(
          siblings.map(async sibling => {
            return await annotatePackageDirMatch(sibling)
          })
        )
        const match = matches.filter(el => !el || el.isPackage).pop()
        if (match) {
          // Success - only one match and it is a package dir
          return [match.fullPath]
        } else {
          // no matches
          return []
        }
      } catch (er) {
        return [] // invalid dir: no matching
      }
    }
    // Note: there used to be registry completion here,
    // but it stopped making sense somewhere around
    // 50,000 packages on the registry
  }

  exec (args, cb) {
    this.install(args).then(() => cb()).catch(cb)
  }

  async install (args) {
    // the /path/to/node_modules/..
    const globalTop = resolve(this.npm.globalDir, '..')
    const { ignoreScripts, global: isGlobalInstall } = this.npm.flatOptions
    const where = isGlobalInstall ? globalTop : this.npm.prefix

    // don't try to install the prefix into itself
    args = args.filter(a => resolve(a) !== this.npm.prefix)

    // `npm i -g` => "install this package globally"
    if (where === globalTop && !args.length)
      args = ['.']

    // TODO: Add warnings for other deprecated flags?  or remove this one?
    if (this.npm.config.get('dev'))
      log.warn('install', 'Usage of the `--dev` option is deprecated. Use `--include=dev` instead.')

    const arb = new Arborist({
      ...this.npm.flatOptions,
      path: where,
    })

    await arb.reify({
      ...this.npm.flatOptions,
      add: args,
    })
    if (!args.length && !isGlobalInstall && !ignoreScripts) {
      const { scriptShell } = this.npm.flatOptions
      const scripts = [
        'preinstall',
        'install',
        'postinstall',
        'prepublish', // XXX should we remove this finally??
        'preprepare',
        'prepare',
        'postprepare',
      ]
      for (const event of scripts) {
        await runScript({
          path: where,
          args: [],
          scriptShell,
          stdio: 'inherit',
          stdioString: true,
          banner: log.level !== 'silent',
          event,
        })
      }
    }
    await reifyFinish(this.npm, arb)
  }
}
module.exports = Install
