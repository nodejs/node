
'use strict'
const fs = require('fs')
const path = require('path')
const YError = require('./yerror')

let previouslyVisitedConfigs = []

function checkForCircularExtends (cfgPath) {
  if (previouslyVisitedConfigs.indexOf(cfgPath) > -1) {
    throw new YError(`Circular extended configurations: '${cfgPath}'.`)
  }
}

function getPathToDefaultConfig (cwd, pathToExtend) {
  return path.resolve(cwd, pathToExtend)
}

function mergeDeep (config1, config2) {
  const target = {}
  const isObject = obj => obj && typeof obj === 'object' && !Array.isArray(obj)
  Object.assign(target, config1)
  for (let key of Object.keys(config2)) {
    if (isObject(config2[key]) && isObject(target[key])) {
      target[key] = mergeDeep(config1[key], config2[key])
    } else {
      target[key] = config2[key]
    }
  }
  return target
}

function applyExtends (config, cwd, mergeExtends) {
  let defaultConfig = {}

  if (Object.prototype.hasOwnProperty.call(config, 'extends')) {
    if (typeof config.extends !== 'string') return defaultConfig
    const isPath = /\.json|\..*rc$/.test(config.extends)
    let pathToDefault = null
    if (!isPath) {
      try {
        pathToDefault = require.resolve(config.extends)
      } catch (err) {
        // most likely this simply isn't a module.
      }
    } else {
      pathToDefault = getPathToDefaultConfig(cwd, config.extends)
    }
    // maybe the module uses key for some other reason,
    // err on side of caution.
    if (!pathToDefault && !isPath) return config

    checkForCircularExtends(pathToDefault)

    previouslyVisitedConfigs.push(pathToDefault)

    defaultConfig = isPath ? JSON.parse(fs.readFileSync(pathToDefault, 'utf8')) : require(config.extends)
    delete config.extends
    defaultConfig = applyExtends(defaultConfig, path.dirname(pathToDefault), mergeExtends)
  }

  previouslyVisitedConfigs = []

  return mergeExtends ? mergeDeep(defaultConfig, config) : Object.assign({}, defaultConfig, config)
}

module.exports = applyExtends
