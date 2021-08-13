export class Info {
  /**
   * @constructor
   * @param {string} property
   * @param {string} attribute
   */
  constructor(property, attribute) {
    this.property = property
    this.attribute = attribute
  }
}

/** @type {string|null} */
Info.prototype.space = null
Info.prototype.attribute = null
Info.prototype.property = null
Info.prototype.boolean = false
Info.prototype.booleanish = false
Info.prototype.overloadedBoolean = false
Info.prototype.number = false
Info.prototype.commaSeparated = false
Info.prototype.spaceSeparated = false
Info.prototype.commaOrSpaceSeparated = false
Info.prototype.mustUseProperty = false
Info.prototype.defined = false
