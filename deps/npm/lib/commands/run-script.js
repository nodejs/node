const { log, output } = require('proc-log')
const pkgJson = require('@npmcli/package-json')
const BaseCommand = require('../base-cmd.js')

class RunScript extends BaseCommand {
  static description = 'Run arbitrary package scripts'
  static params = [
    'workspace',
    'workspaces',
    'include-workspace-root',
    'if-present',
    'ignore-scripts',
    'foreground-scripts',
    'script-shell',
  ]

  static name = 'run-script'
  static usage = ['<command> [-- <args>]']
  static workspaces = true
  static ignoreImplicitWorkspace = false
  static isShellout = true

  static async completion (opts, npm) {
    const argv = opts.conf.argv.remain
    if (argv.length === 2) {
      const { content: { scripts = {} } } = await pkgJson.normalize(npm.localPrefix)
        .catch(() => ({ content: {} }))
      if (opts.isFish) {
        return Object.keys(scripts).map(s => `${s}\t${scripts[s].slice(0, 30)}`)
      }
      return Object.keys(scripts)
    }
  }

  async exec (args) {
    if (args.length) {
      return this.run(args)
    } else {
      return this.list(args)
    }
  }

  async execWorkspaces (args) {
    if (args.length) {
      return this.runWorkspaces(args)
    } else {
      return this.listWorkspaces(args)
    }
  }

  async run ([event, ...args], { path = this.npm.localPrefix, pkg } = {}) {
    const runScript = require('@npmcli/run-script')

    if (!pkg) {
      const { content } = await pkgJson.normalize(path)
      pkg = content
    }
    const { scripts = {} } = pkg

    if (event === 'restart' && !scripts.restart) {
      scripts.restart = 'npm stop --if-present && npm start'
    } else if (event === 'env' && !scripts.env) {
      const { isWindowsShell } = require('../utils/is-windows.js')
      scripts.env = isWindowsShell ? 'SET' : 'env'
    }

    pkg.scripts = scripts

    if (
      !Object.prototype.hasOwnProperty.call(scripts, event) &&
      !(event === 'start' && (await runScript.isServerPackage(path)))
    ) {
      if (this.npm.config.get('if-present')) {
        return
      }

      const didYouMean = require('../utils/did-you-mean.js')
      const suggestions = await didYouMean(path, event)
      throw new Error(
        `Missing script: "${event}"${suggestions}\n\nTo see a list of scripts, run:\n  npm run`
      )
    }

    // positional args only added to the main event, not pre/post
    const events = [[event, args]]
    if (!this.npm.config.get('ignore-scripts')) {
      if (scripts[`pre${event}`]) {
        events.unshift([`pre${event}`, []])
      }

      if (scripts[`post${event}`]) {
        events.push([`post${event}`, []])
      }
    }

    for (const [ev, evArgs] of events) {
      await runScript({
        path,
        // this || undefined is because runScript will be unhappy with the
        // default null value
        scriptShell: this.npm.config.get('script-shell') || undefined,
        stdio: 'inherit',
        pkg,
        event: ev,
        args: evArgs,
      })
    }
  }

  async list (args, path) {
    /* eslint-disable-next-line max-len */
    const { content: { scripts, name, _id } } = await pkgJson.normalize(path || this.npm.localPrefix)
    const pkgid = _id || name

    if (!scripts) {
      return []
    }

    const allScripts = Object.keys(scripts)
    if (this.npm.silent) {
      return allScripts
    }

    if (this.npm.config.get('json')) {
      output.standard(JSON.stringify(scripts, null, 2))
      return allScripts
    }

    if (this.npm.config.get('parseable')) {
      for (const [script, cmd] of Object.entries(scripts)) {
        output.standard(`${script}:${cmd}`)
      }

      return allScripts
    }

    // TODO this is missing things like prepare, prepublishOnly, and dependencies
    const cmdList = [
      'preinstall', 'install', 'postinstall',
      'prepublish', 'publish', 'postpublish',
      'prerestart', 'restart', 'postrestart',
      'prestart', 'start', 'poststart',
      'prestop', 'stop', 'poststop',
      'pretest', 'test', 'posttest',
      'preuninstall', 'uninstall', 'postuninstall',
      'preversion', 'version', 'postversion',
    ]
    const indent = '\n    '
    const prefix = '  '
    const cmds = []
    const runScripts = []
    for (const script of allScripts) {
      const list = cmdList.includes(script) ? cmds : runScripts
      list.push(script)
    }
    const colorize = this.npm.chalk

    if (cmds.length) {
      output.standard(
        `${colorize.reset(colorize.bold('Lifecycle scripts'))} included in ${colorize.green(
          pkgid
        )}:`
      )
    }

    for (const script of cmds) {
      output.standard(prefix + script + indent + colorize.dim(scripts[script]))
    }

    if (!cmds.length && runScripts.length) {
      output.standard(
        `${colorize.bold('Scripts')} available in ${colorize.green(pkgid)} via \`${colorize.blue(
          'npm run-script'
        )}\`:`
      )
    } else if (runScripts.length) {
      output.standard(`\navailable via \`${colorize.blue('npm run-script')}\`:`)
    }

    for (const script of runScripts) {
      output.standard(prefix + script + indent + colorize.dim(scripts[script]))
    }

    output.standard('')
    return allScripts
  }

  async runWorkspaces (args) {
    const res = []
    await this.setWorkspaces()

    for (const workspacePath of this.workspacePaths) {
      const { content: pkg } = await pkgJson.normalize(workspacePath)
      const runResult = await this.run(args, {
        path: workspacePath,
        pkg,
      }).catch(err => {
        log.error(`Lifecycle script \`${args[0]}\` failed with error:`)
        log.error(err)
        log.error(`  in workspace: ${pkg._id || pkg.name}`)
        log.error(`  at location: ${workspacePath}`)
        process.exitCode = 1
      })
      res.push(runResult)
    }
  }

  async listWorkspaces (args) {
    await this.setWorkspaces()

    if (this.npm.silent) {
      return
    }

    if (this.npm.config.get('json')) {
      const res = {}
      for (const workspacePath of this.workspacePaths) {
        const { content: { scripts, name } } = await pkgJson.normalize(workspacePath)
        res[name] = { ...scripts }
      }
      output.standard(JSON.stringify(res, null, 2))
      return
    }

    if (this.npm.config.get('parseable')) {
      for (const workspacePath of this.workspacePaths) {
        const { content: { scripts, name } } = await pkgJson.normalize(workspacePath)
        for (const [script, cmd] of Object.entries(scripts || {})) {
          output.standard(`${name}:${script}:${cmd}`)
        }
      }
      return
    }

    for (const workspacePath of this.workspacePaths) {
      await this.list(args, workspacePath)
    }
  }
}

module.exports = RunScript
