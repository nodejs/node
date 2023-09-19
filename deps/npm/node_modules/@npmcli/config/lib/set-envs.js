// Set environment variables for any non-default configs,
// so that they're already there when we run lifecycle scripts.
//
// See https://github.com/npm/rfcs/pull/90

// Return the env key if this is a thing that belongs in the env.
// Ie, if the key isn't a @scope, //nerf.dart, or _private,
// and the value is a string or array.  Otherwise return false.
const envKey = (key, val) => {
  return !/^[/@_]/.test(key) &&
    (typeof envVal(val) === 'string') &&
      `npm_config_${key.replace(/-/g, '_').toLowerCase()}`
}

const envVal = val => Array.isArray(val) ? val.map(v => envVal(v)).join('\n\n')
  : val === null || val === undefined || val === false ? ''
  : typeof val === 'object' ? null
  : String(val)

const sameConfigValue = (def, val) =>
  !Array.isArray(val) || !Array.isArray(def) ? def === val
  : sameArrayValue(def, val)

const sameArrayValue = (def, val) => {
  if (def.length !== val.length) {
    return false
  }

  for (let i = 0; i < def.length; i++) {
    /* istanbul ignore next - there are no array configs where the default
     * is not an empty array, so this loop is a no-op, but it's the correct
     * thing to do if we ever DO add a config like that. */
    if (def[i] !== val[i]) {
      return false
    }
  }
  return true
}

const setEnv = (env, rawKey, rawVal) => {
  const val = envVal(rawVal)
  const key = envKey(rawKey, val)
  if (key && val !== null) {
    env[key] = val
  }
}

const setEnvs = (config) => {
  // This ensures that all npm config values that are not the defaults are
  // shared appropriately with child processes, without false positives.
  const {
    env,
    defaults,
    definitions,
    list: [cliConf, envConf],
  } = config

  env.INIT_CWD = process.cwd()

  // if the key is deprecated, skip it always.
  // if the key is the default value,
  //   if the environ is NOT the default value,
  //     set the environ
  //   else skip it, it's fine
  // if the key is NOT the default value,
  //   if the env is setting it, then leave it (already set)
  //   otherwise, set the env
  const cliSet = new Set(Object.keys(cliConf))
  const envSet = new Set(Object.keys(envConf))
  for (const key in cliConf) {
    const { deprecated, envExport = true } = definitions[key] || {}
    if (deprecated || envExport === false) {
      continue
    }

    if (sameConfigValue(defaults[key], cliConf[key])) {
      // config is the default, if the env thought different, then we
      // have to set it BACK to the default in the environment.
      if (!sameConfigValue(envConf[key], cliConf[key])) {
        setEnv(env, key, cliConf[key])
      }
    } else {
      // config is not the default.  if the env wasn't the one to set
      // it that way, then we have to put it in the env
      if (!(envSet.has(key) && !cliSet.has(key))) {
        setEnv(env, key, cliConf[key])
      }
    }
  }

  // also set some other common nice envs that we want to rely on
  env.HOME = config.home
  env.npm_config_global_prefix = config.globalPrefix
  env.npm_config_local_prefix = config.localPrefix
  if (cliConf.editor) {
    env.EDITOR = cliConf.editor
  }

  // note: this doesn't afect the *current* node process, of course, since
  // it's already started, but it does affect the options passed to scripts.
  if (cliConf['node-options']) {
    env.NODE_OPTIONS = cliConf['node-options']
  }
  env.npm_execpath = config.npmBin
  env.NODE = env.npm_node_execpath = config.execPath
}

module.exports = setEnvs
