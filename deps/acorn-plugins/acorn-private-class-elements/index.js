"use strict"

const acorn = require('internal/deps/acorn/acorn/dist/acorn')
if (acorn.version.indexOf("6.") != 0 && acorn.version.indexOf("6.0.") == 0 && acorn.version.indexOf("7.") != 0) {
  throw new Error(`acorn-private-class-elements requires acorn@^6.1.0 or acorn@7.0.0, not ${acorn.version}`)
}
const tt = acorn.tokTypes
const TokenType = acorn.TokenType

module.exports = function(Parser) {
  // Only load this plugin once.
  if (Parser.prototype.parsePrivateName) {
    return Parser
  }

  // Make sure `Parser` comes from the same acorn as our `tt`,
  // otherwise the comparisons fail.
  let cur = Parser
  while (cur && cur !== acorn.Parser) {
    cur = cur.__proto__
  }
  if (cur !== acorn.Parser) {
    throw new Error("acorn-private-class-elements does not support mixing different acorn copies")
  }

  Parser = class extends Parser {
    _branch() {
      this.__branch = this.__branch || new Parser({ecmaVersion: this.options.ecmaVersion}, this.input)
      this.__branch.end = this.end
      this.__branch.pos = this.pos
      this.__branch.type = this.type
      this.__branch.value = this.value
      this.__branch.containsEsc = this.containsEsc
      return this.__branch
    }

    parsePrivateClassElementName(element) {
      element.computed = false
      element.key = this.parsePrivateName()
      if (element.key.name == "constructor") this.raise(element.key.start, "Classes may not have a private element named constructor")
      const accept = {get: "set", set: "get"}[element.kind]
      const privateBoundNames = this._privateBoundNamesStack[this._privateBoundNamesStack.length - 1]
      if (Object.prototype.hasOwnProperty.call(privateBoundNames, element.key.name) && privateBoundNames[element.key.name] !== accept) {
        this.raise(element.start, "Duplicate private element")
      }
      privateBoundNames[element.key.name] = element.kind || true
      delete this._unresolvedPrivateNamesStack[this._unresolvedPrivateNamesStack.length - 1][element.key.name]
      return element.key
    }

    parsePrivateName() {
      const node = this.startNode()
      node.name = this.value
      this.next()
      this.finishNode(node, "PrivateName")
      if (this.options.allowReserved == "never") this.checkUnreserved(node)
      return node
    }

    // Parse # token
    getTokenFromCode(code) {
      if (code === 35) {
        ++this.pos
        const word = this.readWord1()
        return this.finishToken(this.privateNameToken, word)
      }
      return super.getTokenFromCode(code)
    }

    // Manage stacks and check for undeclared private names
    parseClass(node, isStatement) {
      this._privateBoundNamesStack = this._privateBoundNamesStack || []
      const privateBoundNames = Object.create(this._privateBoundNamesStack[this._privateBoundNamesStack.length - 1] || null)
      this._privateBoundNamesStack.push(privateBoundNames)
      this._unresolvedPrivateNamesStack = this._unresolvedPrivateNamesStack || []
      const unresolvedPrivateNames = Object.create(null)
      this._unresolvedPrivateNamesStack.push(unresolvedPrivateNames)
      const _return = super.parseClass(node, isStatement)
      this._privateBoundNamesStack.pop()
      this._unresolvedPrivateNamesStack.pop()
      if (!this._unresolvedPrivateNamesStack.length) {
        const names = Object.keys(unresolvedPrivateNames)
        if (names.length) {
          names.sort((n1, n2) => unresolvedPrivateNames[n1] - unresolvedPrivateNames[n2])
          this.raise(unresolvedPrivateNames[names[0]], "Usage of undeclared private name")
        }
      } else Object.assign(this._unresolvedPrivateNamesStack[this._unresolvedPrivateNamesStack.length - 1], unresolvedPrivateNames)
      return _return
    }

    // Parse private element access
    parseSubscript(base, startPos, startLoc, noCalls, maybeAsyncArrow) {
      if (!this.eat(tt.dot)) {
        return super.parseSubscript(base, startPos, startLoc, noCalls, maybeAsyncArrow)
      }
      let node = this.startNodeAt(startPos, startLoc)
      node.object = base
      node.computed = false
      if (this.type == this.privateNameToken) {
        node.property = this.parsePrivateName()
        if (!this._privateBoundNamesStack.length || !this._privateBoundNamesStack[this._privateBoundNamesStack.length - 1][node.property.name]) {
          this._unresolvedPrivateNamesStack[this._unresolvedPrivateNamesStack.length - 1][node.property.name] = node.property.start
        }
      } else {
        node.property = this.parseIdent(true)
      }
      return this.finishNode(node, "MemberExpression")
    }

    // Prohibit delete of private class elements
    parseMaybeUnary(refDestructuringErrors, sawUnary) {
      const _return = super.parseMaybeUnary(refDestructuringErrors, sawUnary)
      if (_return.operator == "delete") {
        if (_return.argument.type == "MemberExpression" && _return.argument.property.type == "PrivateName") {
          this.raise(_return.start, "Private elements may not be deleted")
        }
      }
      return _return
    }
  }
  Parser.prototype.privateNameToken = new TokenType("privateName")
  return Parser
}
