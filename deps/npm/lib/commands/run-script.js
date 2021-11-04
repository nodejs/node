const { resolve } = require('path')
const chalk = require('chalk')
const runScript = require('@npmcli/run-script')
const { isServerPackage } = runScript
const rpj = require('read-package-json-fast')
const log = require('npmlog')
const didYouMean = require('../utils/did-you-mean.js')
const isWindowsShell = require('../utils/is-windows-shell.js')

const cmdList = [
  'publish',
  'install',
  'uninstall',
  'test',
  'stop',
  'start',
  'restart',
  'version',
].reduce((l, p) => l.concat(['pre' + p, p, 'post' + p]), [])

const nocolor = {
  reset: s => s,
  bold: s => s,
  dim: s => s,
  blue: s => s,
  green: s => s,
}

const BaseCommand = require('../base-command.js')
class RunScript extends BaseCommand {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get description () {
    return 'Run arbitrary package scripts'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get params () {
    return [
      'workspace',
      'workspaces',
      'include-workspace-root',
      'if-present',
      'ignore-scripts',
      'script-shell',
    ]
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'run-script'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get usage () {
    return ['<command> [-- <args>]']
  }

  async completion (opts) {
    const argv = opts.conf.argv.remain
    if (argv.length === 2) {
      // find the script name
      const json = resolve(this.npm.localPrefix, 'package.json')
      const { scripts = {} } = await rpj(json).catch(er => ({}))
      return Object.keys(scripts)
    }
  }

  async exec (args) {
    if (args.length)
      return this.run(args)
    else
      return this.list(args)
  }

  async execWorkspaces (args, filters) {
    if (args.length)
      return this.runWorkspaces(args, filters)
    else
      return this.listWorkspaces(args, filters)
  }

  async run ([event, ...args], { path = this.npm.localPrefix, pkg } = {}) {
    // this || undefined is because runScript will be unhappy with the default
    // null value
    const scriptShell = this.npm.config.get('script-shell') || undefined

    pkg = pkg || (await rpj(`${path}/package.json`))
    const { scripts = {} } = pkg

    if (event === 'restart' && !scripts.restart)
      scripts.restart = 'npm stop --if-present && npm start'
    else if (event === 'env' && !scripts.env)
      scripts.env = isWindowsShell ? 'SET' : 'env'

    pkg.scripts = scripts

    if (
      !Object.prototype.hasOwnProperty.call(scripts, event) &&
      !(event === 'start' && await isServerPackage(path))
    ) {
      if (this.npm.config.get('if-present'))
        return

      const suggestions = await didYouMean(this.npm, path, event)
      throw new Error(`Missing script: "${event}"${suggestions}\n\nTo see a list of scripts, run:\n  npm run`)
    }

    // positional args only added to the main event, not pre/post
    const events = [[event, args]]
    if (!this.npm.config.get('ignore-scripts')) {
      if (scripts[`pre${event}`])
        events.unshift([`pre${event}`, []])

      if (scripts[`post${event}`])
        events.push([`post${event}`, []])
    }

    const opts = {
      path,
      args,
      scriptShell,
      stdio: 'inherit',
      stdioString: true,
      pkg,
      banner: log.level !== 'silent',
    }

    for (const [event, args] of events) {
      await runScript({
        ...opts,
        event,
        args,
      })
    }
  }

  async list (args, path) {
    path = path || this.npm.localPrefix
    const { scripts, name, _id } = await rpj(`${path}/package.json`)
    const pkgid = _id || name
    const color = this.npm.color

    if (!scripts)
      return []

    const allScripts = Object.keys(scripts)
    if (log.level === 'silent')
      return allScripts

    if (this.npm.config.get('json')) {
      this.npm.output(JSON.stringify(scripts, null, 2))
      return allScripts
    }

    if (this.npm.config.get('parseable')) {
      for (const [script, cmd] of Object.entries(scripts))
        this.npm.output(`${script}:${cmd}`)

      return allScripts
    }

    const indent = '\n    '
    const prefix = '  '
    const cmds = []
    const runScripts = []
    for (const script of allScripts) {
      const list = cmdList.includes(script) ? cmds : runScripts
      list.push(script)
    }
    const colorize = color ? chalk : nocolor

    if (cmds.length) {
      this.npm.output(`${
        colorize.reset(colorize.bold('Lifecycle scripts'))} included in ${
        colorize.green(pkgid)}:`)
    }

    for (const script of cmds)
      this.npm.output(prefix + script + indent + colorize.dim(scripts[script]))

    if (!cmds.length && runScripts.length) {
      this.npm.output(`${
        colorize.bold('Scripts')
      } available in ${colorize.green(pkgid)} via \`${
        colorize.blue('npm run-script')}\`:`)
    } else if (runScripts.length)
      this.npm.output(`\navailable via \`${colorize.blue('npm run-script')}\`:`)

    for (const script of runScripts)
      this.npm.output(prefix + script + indent + colorize.dim(scripts[script]))

    this.npm.output('')
    return allScripts
  }

  async runWorkspaces (args, filters) {
    const res = []
    await this.setWorkspaces(filters)

    for (const workspacePath of this.workspacePaths) {
      const pkg = await rpj(`${workspacePath}/package.json`)
      const runResult = await this.run(args, {
        path: workspacePath,
        pkg,
      }).catch(err => {
        log.error(`Lifecycle script \`${args[0]}\` failed with error:`)
        log.error(err)
        log.error(`  in workspace: ${pkg._id || pkg.name}`)
        log.error(`  at location: ${workspacePath}`)

        const scriptMissing = err.message.startsWith('Missing script')

        // avoids exiting with error code in case there's scripts missing
        // in some workspaces since other scripts might have succeeded
        if (!scriptMissing)
          process.exitCode = 1

        return scriptMissing
      })
      res.push(runResult)
    }

    // in case **all** tests are missing, then it should exit with error code
    if (res.every(Boolean))
      throw new Error(`Missing script: ${args[0]}`)
  }

  async listWorkspaces (args, filters) {
    await this.setWorkspaces(filters)

    if (log.level === 'silent')
      return

    if (this.npm.config.get('json')) {
      const res = {}
      for (const workspacePath of this.workspacePaths) {
        const { scripts, name } = await rpj(`${workspacePath}/package.json`)
        res[name] = { ...scripts }
      }
      this.npm.output(JSON.stringify(res, null, 2))
      return
    }

    if (this.npm.config.get('parseable')) {
      for (const workspacePath of this.workspacePaths) {
        const { scripts, name } = await rpj(`${workspacePath}/package.json`)
        for (const [script, cmd] of Object.entries(scripts || {}))
          this.npm.output(`${name}:${script}:${cmd}`)
      }
      return
    }

    for (const workspacePath of this.workspacePaths)
      await this.list(args, workspacePath)
  }
}

module.exports = RunScript
