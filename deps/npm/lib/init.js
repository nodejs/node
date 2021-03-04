const initJson = require('init-package-json')
const npa = require('npm-package-arg')

const usageUtil = require('./utils/usage.js')
const output = require('./utils/output.js')

class Init {
  constructor (npm) {
    this.npm = npm
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  get usage () {
    return usageUtil(
      'init',
      '\nnpm init [--force|-f|--yes|-y|--scope]' +
      '\nnpm init <@scope> (same as `npx <@scope>/create`)' +
      '\nnpm init [<@scope>/]<name> (same as `npx [<@scope>/]create-<name>`)'
    )
  }

  exec (args, cb) {
    this.init(args).then(() => cb()).catch(cb)
  }

  async init (args) {
    // the new npx style way
    if (args.length) {
      const initerName = args[0]
      let packageName = initerName
      if (/^@[^/]+$/.test(initerName))
        packageName = initerName + '/create'
      else {
        const req = npa(initerName)
        if (req.type === 'git' && req.hosted) {
          const { user, project } = req.hosted
          packageName = initerName
            .replace(user + '/' + project, user + '/create-' + project)
        } else if (req.registry) {
          packageName = req.name.replace(/^(@[^/]+\/)?/, '$1create-')
          if (req.rawSpec)
            packageName += '@' + req.rawSpec
        } else {
          throw Object.assign(new Error(
            'Unrecognized initializer: ' + initerName +
            '\nFor more package binary executing power check out `npx`:' +
            '\nhttps://www.npmjs.com/package/npx'
          ), { code: 'EUNSUPPORTED' })
        }
      }
      this.npm.config.set('package', [])
      const newArgs = [packageName, ...args.slice(1)]
      return new Promise((res, rej) => {
        this.npm.commands.exec(newArgs, er => er ? rej(er) : res())
      })
    }

    // the old way
    const dir = process.cwd()
    this.npm.log.pause()
    this.npm.log.disableProgress()
    const initFile = this.npm.config.get('init-module')
    if (!this.npm.flatOptions.yes && !this.npm.flatOptions.force) {
      output([
        'This utility will walk you through creating a package.json file.',
        'It only covers the most common items, and tries to guess sensible defaults.',
        '',
        'See `npm help init` for definitive documentation on these fields',
        'and exactly what they do.',
        '',
        'Use `npm install <pkg>` afterwards to install a package and',
        'save it as a dependency in the package.json file.',
        '',
        'Press ^C at any time to quit.',
      ].join('\n'))
    }
    // XXX promisify init-package-json
    await new Promise((res, rej) => {
      initJson(dir, initFile, this.npm.config, (er, data) => {
        this.npm.log.resume()
        this.npm.log.enableProgress()
        this.npm.log.silly('package data', data)
        if (er && er.message === 'canceled') {
          this.npm.log.warn('init', 'canceled')
          return res()
        }
        if (er)
          rej(er)
        else {
          this.npm.log.info('init', 'written successfully')
          res(data)
        }
      })
    })
  }
}
module.exports = Init
