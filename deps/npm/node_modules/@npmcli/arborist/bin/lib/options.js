const options = module.exports = {
  path: undefined,
  cache: `${process.env.HOME}/.npm/_cacache`,
  _: [],
}

for (const arg of process.argv.slice(2)) {
  if (/^--add=/.test(arg)) {
    options.add = options.add || []
    options.add.push(arg.substr('--add='.length))
  } else if (/^--rm=/.test(arg)) {
    options.rm = options.rm || []
    options.rm.push(arg.substr('--rm='.length))
  } else if (arg === '--global')
    options.global = true
  else if (arg === '--global-style')
    options.globalStyle = true
  else if (arg === '--prefer-dedupe')
    options.preferDedupe = true
  else if (arg === '--legacy-peer-deps')
    options.legacyPeerDeps = true
  else if (arg === '--force')
    options.force = true
  else if (arg === '--update-all') {
    options.update = options.update || {}
    options.update.all = true
  } else if (/^--update=/.test(arg)) {
    options.update = options.update || {}
    options.update.names = options.update.names || []
    options.update.names.push(arg.substr('--update='.length))
  } else if (/^--omit=/.test(arg)) {
    options.omit = options.omit || []
    options.omit.push(arg.substr('--omit='.length))
  } else if (/^--before=/.test(arg))
    options.before = new Date(arg.substr('--before='.length))
  else if (/^--[^=]+=/.test(arg)) {
    const [key, ...v] = arg.replace(/^--/, '').split('=')
    const val = v.join('=')
    options[key] = val === 'false' ? false : val === 'true' ? true : val
  } else if (/^--.+/.test(arg))
    options[arg.replace(/^--/, '')] = true
  else if (options.path === undefined)
    options.path = arg
  else
    options._.push(arg)
}

if (options.path === undefined)
  options.path = '.'

console.error(options)
