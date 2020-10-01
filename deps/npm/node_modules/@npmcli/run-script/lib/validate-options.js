const validateOptions = options => {
  if (typeof options !== 'object' || !options)
    throw new TypeError('invalid options object provided to runScript')

  const {
    event,
    path,
    scriptShell,
    env = {},
    stdio = 'pipe',
    pkg,
    args = [],
    cmd,
  } = options

  if (!event || typeof event !== 'string')
    throw new TypeError('valid event not provided to runScript')
  if (!path || typeof path !== 'string')
    throw new TypeError('valid path not provided to runScript')
  if (scriptShell !== undefined && typeof scriptShell !== 'string')
    throw new TypeError('invalid scriptShell option provided to runScript')
  if (typeof env !== 'object' || !env)
    throw new TypeError('invalid env option provided to runScript')
  if (typeof stdio !== 'string' && !Array.isArray(stdio))
    throw new TypeError('invalid stdio option provided to runScript')
  if (!Array.isArray(args) || args.some(a => typeof a !== 'string'))
    throw new TypeError('invalid args option provided to runScript')
  if (cmd !== undefined && typeof cmd !== 'string')
    throw new TypeError('invalid cmd option provided to runScript')
}

module.exports = validateOptions
