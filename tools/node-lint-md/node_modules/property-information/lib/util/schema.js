/**
 * @typedef {import('./info.js').Info} Info
 * @typedef {Object.<string, Info>} Properties
 * @typedef {Object.<string, string>} Normal
 */

export class Schema {
  /**
   * @constructor
   * @param {Properties} property
   * @param {Normal} normal
   * @param {string} [space]
   */
  constructor(property, normal, space) {
    this.property = property
    this.normal = normal
    if (space) {
      this.space = space
    }
  }
}

/** @type {Properties} */
Schema.prototype.property = {}
/** @type {Normal} */
Schema.prototype.normal = {}
/** @type {string|null} */
Schema.prototype.space = null
