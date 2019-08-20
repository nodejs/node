"use strict"

const acorn = require('internal/deps/acorn/acorn/dist/acorn')
const tt = acorn.tokTypes
const privateClassElements = require('internal/deps/acorn-plugins/acorn-private-class-elements/index')

function maybeParseFieldValue(field) {
  if (this.eat(tt.eq)) {
    const oldInFieldValue = this._inFieldValue
    this._inFieldValue = true
    field.value = this.parseExpression()
    this._inFieldValue = oldInFieldValue
  } else field.value = null
}

module.exports = function(Parser) {
  Parser = privateClassElements(Parser)
  return class extends Parser {
    // Parse fields
    parseClassElement(_constructorAllowsSuper) {
      if (this.options.ecmaVersion >= 8 && (this.type == tt.name || this.type == this.privateNameToken || this.type == tt.bracketL || this.type == tt.string)) {
        const branch = this._branch()
        if (branch.type == tt.bracketL) {
          let count = 0
          do {
            if (branch.eat(tt.bracketL)) ++count
            else if (branch.eat(tt.bracketR)) --count
            else branch.next()
          } while (count > 0)
        } else branch.next()
        if (branch.type == tt.eq || branch.canInsertSemicolon() || branch.type == tt.semi) {
          const node = this.startNode()
          if (this.type == this.privateNameToken) {
            this.parsePrivateClassElementName(node)
          } else {
            this.parsePropertyName(node)
          }
          if ((node.key.type === "Identifier" && node.key.name === "constructor") ||
              (node.key.type === "Literal" && node.key.value === "constructor")) {
            this.raise(node.key.start, "Classes may not have a field called constructor")
          }
          maybeParseFieldValue.call(this, node)
          this.finishNode(node, "FieldDefinition")
          this.semicolon()
          return node
        }
      }

      return super.parseClassElement.apply(this, arguments)
    }

    // Prohibit arguments in class field initializers
    parseIdent(liberal, isBinding) {
      const ident = super.parseIdent(liberal, isBinding)
      if (this._inFieldValue && ident.name == "arguments") this.raise(ident.start, "A class field initializer may not contain arguments")
      return ident
    }
  }
}
