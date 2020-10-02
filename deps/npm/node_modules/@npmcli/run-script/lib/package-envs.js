// https://github.com/npm/rfcs/pull/183

const envVal = val => Array.isArray(val) ? val.map(v => envVal(v)).join('\n\n')
  : val === null || val === false ? ''
  : String(val)

const packageEnvs = (env, vals, prefix) => {
  for (const [key, val] of Object.entries(vals)) {
    if (val === undefined)
      continue
    else if (val && !Array.isArray(val) && typeof val === 'object')
      packageEnvs(env, val, `${prefix}${key}_`)
    else
      env[`${prefix}${key}`] = envVal(val)
  }
  return env
}

module.exports = (env, pkg) => packageEnvs({ ...env }, {
  name: pkg.name,
  version: pkg.version,
  config: pkg.config,
  engines: pkg.engines,
  bin: pkg.bin,
}, 'npm_package_')
