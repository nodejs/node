const fs = require('fs')
const { relative, resolve } = require('path')
const mkdirp = require('mkdirp-infer-owner')
const initJson = require('init-package-json')
const npa = require('npm-package-arg')
const rpj = require('read-package-json-fast')
const libexec = require('libnpmexec')
const parseJSON = require('json-parse-even-better-errors')
const mapWorkspaces = require('@npmcli/map-workspaces')

const getLocationMsg = require('./exec/get-workspace-location-msg.js')
const BaseCommand = require('./base-command.js')

class Init extends BaseCommand {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get description () {
    return 'Create a package.json file'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get params () {
    return ['yes', 'force', 'workspace', 'workspaces']
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'init'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get usage () {
    return [
      '[--force|-f|--yes|-y|--scope]',
      '<@scope> (same as `npx <@scope>/create`)',
      '[<@scope>/]<name> (same as `npx [<@scope>/]create-<name>`)',
    ]
  }

  exec (args, cb) {
    this.init(args).then(() => cb()).catch(cb)
  }

  execWorkspaces (args, filters, cb) {
    this.initWorkspaces(args, filters).then(() => cb()).catch(cb)
  }

  async init (args) {
    // npm exec style
    if (args.length)
      return (await this.execCreate({ args, path: process.cwd() }))

    // no args, uses classic init-package-json boilerplate
    await this.template()
  }

  async initWorkspaces (args, filters) {
    // reads package.json for the top-level folder first, by doing this we
    // ensure the command throw if no package.json is found before trying
    // to create a workspace package.json file or its folders
    const pkg = await rpj(resolve(this.npm.localPrefix, 'package.json'))
    const wPath = filterArg => resolve(this.npm.localPrefix, filterArg)

    // npm-exec style, runs in the context of each workspace filter
    if (args.length) {
      for (const filterArg of filters) {
        const path = wPath(filterArg)
        await mkdirp(path)
        await this.execCreate({ args, path })
        await this.setWorkspace({ pkg, workspacePath: path })
      }
      return
    }

    // no args, uses classic init-package-json boilerplate
    for (const filterArg of filters) {
      const path = wPath(filterArg)
      await mkdirp(path)
      await this.template(path)
      await this.setWorkspace({ pkg, workspacePath: path })
    }
  }

  async execCreate ({ args, path }) {
    const [initerName, ...otherArgs] = args
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

    const newArgs = [packageName, ...otherArgs]
    const cache = this.npm.config.get('cache')
    const { color } = this.npm.flatOptions
    const {
      flatOptions,
      localBin,
      log,
      globalBin,
      output,
    } = this.npm
    const locationMsg = await getLocationMsg({ color, path })
    const runPath = path
    const scriptShell = this.npm.config.get('script-shell') || undefined
    const yes = this.npm.config.get('yes')

    await libexec({
      ...flatOptions,
      args: newArgs,
      cache,
      color,
      localBin,
      locationMsg,
      log,
      globalBin,
      output,
      path,
      runPath,
      scriptShell,
      yes,
    })
  }

  async template (path = process.cwd()) {
    this.npm.log.pause()
    this.npm.log.disableProgress()

    const initFile = this.npm.config.get('init-module')
    if (!this.npm.config.get('yes') && !this.npm.config.get('force')) {
      this.npm.output([
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
      initJson(path, initFile, this.npm.config, (er, data) => {
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

  async setWorkspace ({ pkg, workspacePath }) {
    const workspaces = await mapWorkspaces({ cwd: this.npm.localPrefix, pkg })

    // skip setting workspace if current package.json glob already satisfies it
    for (const wPath of workspaces.values()) {
      if (wPath === workspacePath)
        return
    }

    // if a create-pkg didn't generate a package.json at the workspace
    // folder level, it might not be recognized as a workspace by
    // mapWorkspaces, so we're just going to avoid touching the
    // top-level package.json
    try {
      fs.statSync(resolve(workspacePath, 'package.json'))
    } catch (err) {
      return
    }

    let manifest
    try {
      manifest =
        fs.readFileSync(resolve(this.npm.localPrefix, 'package.json'), 'utf-8')
    } catch (error) {
      throw new Error('package.json not found')
    }

    try {
      manifest = parseJSON(manifest)
    } catch (error) {
      throw new Error(`Invalid package.json: ${error}`)
    }

    if (!manifest.workspaces)
      manifest.workspaces = []

    manifest.workspaces.push(relative(this.npm.localPrefix, workspacePath))

    // format content
    const {
      [Symbol.for('indent')]: indent,
      [Symbol.for('newline')]: newline,
    } = manifest

    const content = (JSON.stringify(manifest, null, indent) + '\n')
      .replace(/\n/g, newline)

    fs.writeFileSync(resolve(this.npm.localPrefix, 'package.json'), content)
  }
}

module.exports = Init
