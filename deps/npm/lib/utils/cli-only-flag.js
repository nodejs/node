// Read a config value only when it was passed on the command line.
// Values from .npmrc, env, or defaults resolve to undefined, so the flag cannot be set as project policy.
const cliOnlyFlag = (config, key) =>
  config.find(key) === 'cli' ? config.get(key) : undefined

// The patch relax flags, honored only from the command line, as Arborist options.
const patchRelaxOpts = config => ({
  allowUnusedPatches: cliOnlyFlag(config, 'allow-unused-patches'),
  ignorePatchFailures: cliOnlyFlag(config, 'ignore-patch-failures'),
})

module.exports = cliOnlyFlag
module.exports.patchRelaxOpts = patchRelaxOpts
