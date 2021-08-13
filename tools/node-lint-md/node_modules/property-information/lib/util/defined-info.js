import {Info} from './info.js'
import * as types from './types.js'

var checks = Object.keys(types)

export class DefinedInfo extends Info {
  /**
   * @constructor
   * @param {string} property
   * @param {string} attribute
   * @param {number} [mask]
   * @param {string} [space]
   */
  constructor(property, attribute, mask, space) {
    var index = -1

    super(property, attribute)

    mark(this, 'space', space)

    while (++index < checks.length) {
      mark(
        this,
        checks[index],
        (mask & types[checks[index]]) === types[checks[index]]
      )
    }
  }
}

DefinedInfo.prototype.defined = true

/**
 * @param {InstanceType<typeof DefinedInfo>} values
 * @param {string} key
 * @param {unknown} value
 */
function mark(values, key, value) {
  if (value) {
    values[key] = value
  }
}
