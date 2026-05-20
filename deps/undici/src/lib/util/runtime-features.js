'use strict'

/** @typedef {`node:${string}`} NodeModuleName */

/** @type {Record<NodeModuleName, () => any>} */
const lazyLoaders = {
  __proto__: null,
  'node:crypto': () => require('node:crypto'),
  'node:sqlite': () => require('node:sqlite')
}

/**
 * @param {NodeModuleName} moduleName
 * @returns {boolean}
 */
function detectRuntimeFeatureByNodeModule (moduleName) {
  try {
    lazyLoaders[moduleName]()
    return true
  } catch (err) {
    if (err.code !== 'ERR_UNKNOWN_BUILTIN_MODULE' && err.code !== 'ERR_NO_CRYPTO') {
      throw err
    }
    return false
  }
}

const runtimeFeaturesAsNodeModule = /** @type {const} */ (['crypto', 'sqlite'])
/** @typedef {typeof runtimeFeaturesAsNodeModule[number]} RuntimeFeatureByNodeModule */
/** @typedef {RuntimeFeatureByNodeModule} Feature */

/**
 * @param {Feature} feature
 * @returns {boolean}
 */
function detectRuntimeFeature (feature) {
  if (runtimeFeaturesAsNodeModule.includes(/** @type {RuntimeFeatureByNodeModule} */ (feature))) {
    return detectRuntimeFeatureByNodeModule(`node:${feature}`)
  }
  throw new TypeError(`unknown feature: ${feature}`)
}

/**
 * @class
 * @name RuntimeFeatures
 */
class RuntimeFeatures {
  /** @type {Map<Feature, boolean>} */
  #map = new Map()

  /**
   * Clears all cached feature detections.
   */
  clear () {
    this.#map.clear()
  }

  /**
   * @param {Feature} feature
   * @returns {boolean}
   */
  has (feature) {
    return (
      this.#map.get(feature) ?? this.#detectRuntimeFeature(feature)
    )
  }

  /**
   * @param {Feature} feature
   * @param {boolean} value
   */
  set (feature, value) {
    if (runtimeFeaturesAsNodeModule.includes(feature) === false) {
      throw new TypeError(`unknown feature: ${feature}`)
    }
    this.#map.set(feature, value)
  }

  /**
   * @param {Feature} feature
   * @returns {boolean}
   */
  #detectRuntimeFeature (feature) {
    const result = detectRuntimeFeature(feature)
    this.#map.set(feature, result)
    return result
  }
}

const instance = new RuntimeFeatures()

module.exports.runtimeFeatures = instance
module.exports.default = instance
