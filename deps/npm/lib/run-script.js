const runScript = require('@npmcli/run-script')
const { isServerPackage } = runScript
const npm = require('./npm.js')
const readJson = require('read-package-json-fast')
const { resolve } = require('path')
const output = require('./utils/output.js')
const log = require('npmlog')
const usageUtil = require('./utils/usage')
const didYouMean = require('./utils/did-you-mean')
const isWindowsShell = require('./utils/is-windows-shell.js')

const usage = usageUtil(
  'run-script',
  'npm run-script <command> [-- <args>]'
)

const completion = async (opts) => {
  const argv = opts.conf.argv.remain
  if (argv.length === 2) {
    // find the script name
    const json = resolve(npm.localPrefix, 'package.json')
    const { scripts = {} } = await readJson(json).catch(er => ({}))
    return Object.keys(scripts)
  }
}

const cmd = (args, cb) => {
  const fn = args.length ? doRun : list
  return fn(args).then(() => cb()).catch(cb)
}

const doRun = async (args) => {
  const path = npm.localPrefix
  const event = args.shift()
  const { scriptShell } = npm.flatOptions

  const pkg = await readJson(`${path}/package.json`)
  const { scripts = {} } = pkg

  if (event === 'restart' && !scripts.restart)
    scripts.restart = 'npm stop --if-present && npm start'
  else if (event === 'env' && !scripts.env)
    scripts.env = isWindowsShell ? 'SET' : 'env'

  pkg.scripts = scripts

  if (!Object.prototype.hasOwnProperty.call(scripts, event) && !(event === 'start' && await isServerPackage(path))) {
    if (npm.config.get('if-present'))
      return

    const suggestions = didYouMean(event, Object.keys(scripts))
    throw new Error(`missing script: ${event}${
      suggestions ? `\n${suggestions}` : ''}`)
  }

  // positional args only added to the main event, not pre/post
  const events = [[event, args]]
  if (!npm.flatOptions.ignoreScripts) {
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

const list = async () => {
  const path = npm.localPrefix
  const { scripts, name } = await readJson(`${path}/package.json`)
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

  if (!scripts)
    return []

  const allScripts = Object.keys(scripts)
  if (log.level === 'silent')
    return allScripts

  if (npm.flatOptions.json) {
    output(JSON.stringify(scripts, null, 2))
    return allScripts
  }

  if (npm.flatOptions.parseable) {
    for (const [script, cmd] of Object.entries(scripts))
      output(`${script}:${cmd}`)

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

  if (cmds.length)
    output(`Lifecycle scripts included in ${name}:`)

  for (const script of cmds)
    output(prefix + script + indent + scripts[script])

  if (!cmds.length && runScripts.length)
    output(`Scripts available in ${name} via \`npm run-script\`:`)
  else if (runScripts.length)
    output('\navailable via `npm run-script`:')

  for (const script of runScripts)
    output(prefix + script + indent + scripts[script])

  return allScripts
}

module.exports = Object.assign(cmd, { completion, usage })
