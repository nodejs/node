"use strict"

const privateClassElements = require('internal/deps/acorn-plugins/acorn-private-class-elements/index')

module.exports = function(Parser) {
  const ExtendedParser = privateClassElements(Parser)

  return class extends ExtendedParser {
    // Parse private methods
    parseClassElement(_constructorAllowsSuper) {
      const oldInClassMemberName = this._inClassMemberName
      this._inClassMemberName = true
      const result = super.parseClassElement.apply(this, arguments)
      this._inClassMemberName = oldInClassMemberName
      return result
    }

    parsePropertyName(prop) {
      const isPrivate = this.options.ecmaVersion >= 8 && this._inClassMemberName && this.type == this.privateNameToken
      this._inClassMemberName = false
      if (!isPrivate) return super.parsePropertyName(prop)
      return this.parsePrivateClassElementName(prop)
    }
  }
}
