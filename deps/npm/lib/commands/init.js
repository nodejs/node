const fs = require('fs')
const { relative, resolve } = require('path')
const mkdirp = require('mkdirp-infer-owner')
const initJson = require('init-package-json')
const npa = require('npm-package-arg')
const rpj = require('read-package-json-fast')
const libexec = require('libnpmexec')
const mapWorkspaces = require('@npmcli/map-workspaces')
const PackageJson = require('@npmcli/package-json')
const log = require('../utils/log-shim.js')
const updateWorkspaces = require('../workspaces/update-workspaces.js')

const getLocationMsg = require('../exec/get-workspace-location-msg.js')
const BaseCommand = require('../base-command.js')

class Init extends BaseCommand {
  static description = 'Create a package.json file'
  static params = [
    'yes',
    'force',
    'workspace',
    'workspaces',
    'workspaces-update',
    'include-workspace-root',
  ]

  static name = 'init'
  static usage = [
    '[--force|-f|--yes|-y|--scope]',
    '<@scope> (same as `npx <@scope>/create`)',
    '[<@scope>/]<name> (same as `npx [<@scope>/]create-<name>`)',
  ]

  static ignoreImplicitWorkspace = false

  async exec (args) {
    // npm exec style
    if (args.length) {
      return (await this.execCreate({ args, path: process.cwd() }))
    }

    // no args, uses classic init-package-json boilerplate
    await this.template()
  }

  async execWorkspaces (args, filters) {
    // if the root package is uninitiated, take care of it first
    if (this.npm.flatOptions.includeWorkspaceRoot) {
      await this.exec(args)
    }

    // reads package.json for the top-level folder first, by doing this we
    // ensure the command throw if no package.json is found before trying
    // to create a workspace package.json file or its folders
    const pkg = await rpj(resolve(this.npm.localPrefix, 'package.json'))
    const wPath = filterArg => resolve(this.npm.localPrefix, filterArg)

    const workspacesPaths = []
    // npm-exec style, runs in the context of each workspace filter
    if (args.length) {
      for (const filterArg of filters) {
        const path = wPath(filterArg)
        await mkdirp(path)
        workspacesPaths.push(path)
        await this.execCreate({ args, path })
        await this.setWorkspace({ pkg, workspacePath: path })
      }
      return
    }

    // no args, uses classic init-package-json boilerplate
    for (const filterArg of filters) {
      const path = wPath(filterArg)
      await mkdirp(path)
      workspacesPaths.push(path)
      await this.template(path)
      await this.setWorkspace({ pkg, workspacePath: path })
    }

    // reify packages once all workspaces have been initialized
    await this.update(workspacesPaths)
  }

  async execCreate ({ args, path }) {
    const [initerName, ...otherArgs] = args
    let packageName = initerName

    if (/^@[^/]+$/.test(initerName)) {
      packageName = initerName + '/create'
    } else {
      const req = npa(initerName)
      if (req.type === 'git' && req.hosted) {
        const { user, project } = req.hosted
        packageName = initerName
          .replace(user + '/' + project, user + '/create-' + project)
      } else if (req.registry) {
        packageName = req.name.replace(/^(@[^/]+\/)?/, '$1create-')
        if (req.rawSpec) {
          packageName += '@' + req.rawSpec
        }
      } else {
        throw Object.assign(new Error(
          'Unrecognized initializer: ' + initerName +
          '\nFor more package binary executing power check out `npx`:' +
          '\nhttps://www.npmjs.com/package/npx'
        ), { code: 'EUNSUPPORTED' })
      }
    }

    const newArgs = [packageName, ...otherArgs]
    const { color } = this.npm.flatOptions
    const {
      flatOptions,
      localBin,
      globalBin,
    } = this.npm
    // this function is definitely called.  But because of coverage map stuff
    // it ends up both uncovered, and the coverage report doesn't even mention.
    // the tests do assert that some output happens, so we know this line is
    // being hit.
    /* istanbul ignore next */
    const output = (...outputArgs) => this.npm.output(...outputArgs)
    const locationMsg = await getLocationMsg({ color, path })
    const runPath = path
    const scriptShell = this.npm.config.get('script-shell') || undefined
    const yes = this.npm.config.get('yes')

    await libexec({
      ...flatOptions,
      args: newArgs,
      color,
      localBin,
      locationMsg,
      globalBin,
      output,
      path,
      runPath,
      scriptShell,
      yes,
    })
  }

  async template (path = process.cwd()) {
    log.pause()
    log.disableProgress()

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
        log.resume()
        log.enableProgress()
        log.silly('package data', data)
        if (er && er.message === 'canceled') {
          log.warn('init', 'canceled')
          return res()
        }
        if (er) {
          rej(er)
        } else {
          log.info('init', 'written successfully')
          res(data)
        }
      })
    })
  }

  async setWorkspace ({ pkg, workspacePath }) {
    const workspaces = await mapWorkspaces({ cwd: this.npm.localPrefix, pkg })

    // skip setting workspace if current package.json glob already satisfies it
    for (const wPath of workspaces.values()) {
      if (wPath === workspacePath) {
        return
      }
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

    const pkgJson = await PackageJson.load(this.npm.localPrefix)

    pkgJson.update({
      workspaces: [
        ...(pkgJson.content.workspaces || []),
        relative(this.npm.localPrefix, workspacePath),
      ],
    })

    await pkgJson.save()
  }

  async update (workspacesPaths) {
    // translate workspaces paths into an array containing workspaces names
    const workspaces = []
    for (const path of workspacesPaths) {
      const pkgPath = resolve(path, 'package.json')
      const { name } = await rpj(pkgPath)
        .catch(() => ({}))

      if (name) {
        workspaces.push(name)
      }
    }

    const {
      config,
      flatOptions,
      localPrefix,
    } = this.npm

    await updateWorkspaces({
      config,
      flatOptions,
      localPrefix,
      npm: this.npm,
      workspaces,
    })
  }
}

module.exports = Init
