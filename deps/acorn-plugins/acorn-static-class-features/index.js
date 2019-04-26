"use strict"

const skipWhiteSpace = /(?:\s|\/\/.*|\/\*[^]*?\*\/)*/g

const acorn = require('internal/deps/acorn/acorn/dist/acorn')
const tt = acorn.tokTypes

function maybeParseFieldValue(field) {
  if (this.eat(tt.eq)) {
    const oldInFieldValue = this._inStaticFieldValue
    this._inStaticFieldValue = true
    field.value = this.parseExpression()
    this._inStaticFieldValue = oldInFieldValue
  } else field.value = null
}

const privateClassElements = require("internal/deps/acorn-plugins/acorn-private-class-elements/index")

module.exports = function(Parser) {
  const ExtendedParser = privateClassElements(Parser)

  return class extends ExtendedParser {
    // Parse private fields
    parseClassElement(_constructorAllowsSuper) {
      if (this.eat(tt.semi)) return null

      const node = this.startNode()

      const tryContextual = (k, noLineBreak) => {
        if (typeof noLineBreak == "undefined") noLineBreak = false
        const start = this.start, startLoc = this.startLoc
        if (!this.eatContextual(k)) return false
        if (this.type !== tt.parenL && (!noLineBreak || !this.canInsertSemicolon())) return true
        if (node.key) this.unexpected()
        node.computed = false
        node.key = this.startNodeAt(start, startLoc)
        node.key.name = k
        this.finishNode(node.key, "Identifier")
        return false
      }

      node.static = tryContextual("static")
      if (!node.static) return super.parseClassElement.apply(this, arguments)

      let isGenerator = this.eat(tt.star)
      let isAsync = false
      if (!isGenerator) {
        // Special-case for `async`, since `parseClassMember` currently looks
        // for `(` to determine whether `async` is a method name
        if (this.options.ecmaVersion >= 8 && this.isContextual("async")) {
          skipWhiteSpace.lastIndex = this.pos
          let skip = skipWhiteSpace.exec(this.input)
          let next = this.input.charAt(this.pos + skip[0].length)
          if (next === ";" || next === "=") {
            node.key = this.parseIdent(true)
            node.computed = false
            maybeParseFieldValue.call(this, node)
            this.finishNode(node, "FieldDefinition")
            this.semicolon()
            return node
          } else if (this.options.ecmaVersion >= 8 && tryContextual("async", true)) {
            isAsync = true
            isGenerator = this.options.ecmaVersion >= 9 && this.eat(tt.star)
          }
        } else if (tryContextual("get")) {
          node.kind = "get"
        } else if (tryContextual("set")) {
          node.kind = "set"
        }
      }
      if (this.type === this.privateNameToken) {
        this.parsePrivateClassElementName(node)
        if (this.type !== tt.parenL) {
          if (node.key.name === "prototype") {
            this.raise(node.key.start, "Classes may not have a private static property named prototype")
          }
          maybeParseFieldValue.call(this, node)
          this.finishNode(node, "FieldDefinition")
          this.semicolon()
          return node
        }
      } else if (!node.key) {
        this.parsePropertyName(node)
        if ((node.key.name || node.key.value) === "prototype" && !node.computed) {
          this.raise(node.key.start, "Classes may not have a static property named prototype")
        }
      }
      if (!node.kind) node.kind = "method"
      this.parseClassMethod(node, isGenerator, isAsync)
      if (!node.kind && (node.key.name || node.key.value) === "constructor" && !node.computed) {
        this.raise(node.key.start, "Classes may not have a static field named constructor")
      }
      if (node.kind === "get" && node.value.params.length !== 0) {
        this.raiseRecoverable(node.value.start, "getter should have no params")
      }
      if (node.kind === "set" && node.value.params.length !== 1) {
        this.raiseRecoverable(node.value.start, "setter should have exactly one param")
      }
      if (node.kind === "set" && node.value.params[0].type === "RestElement") {
        this.raiseRecoverable(node.value.params[0].start, "Setter cannot use rest params")
      }

      return node

    }

    // Parse public static fields
    parseClassMethod(method, isGenerator, isAsync, _allowsDirectSuper) {
      if (isGenerator || isAsync || method.kind != "method" || !method.static || this.options.ecmaVersion < 8 || this.type == tt.parenL) {
        return super.parseClassMethod.apply(this, arguments)
      }
      maybeParseFieldValue.call(this, method)
      delete method.kind
      method = this.finishNode(method, "FieldDefinition")
      this.semicolon()
      return method
    }

    // Prohibit arguments in class field initializers
    parseIdent(liberal, isBinding) {
      const ident = super.parseIdent(liberal, isBinding)
      if (this._inStaticFieldValue && ident.name == "arguments") this.raise(ident.start, "A static class field initializer may not contain arguments")
      return ident
    }
  }
}
