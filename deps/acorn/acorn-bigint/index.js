"use strict"

const acorn = require('internal/deps/acorn/acorn/dist/acorn')
const tt = acorn.tokTypes
const isIdentifierStart = acorn.isIdentifierStart

module.exports = function(Parser) {
  return class extends Parser {
    parseLiteral(value) {
      const node = super.parseLiteral(value)
      if (node.raw.charCodeAt(node.raw.length - 1) == 110) node.bigint = this.getNumberInput(node.start, node.end)
      return node
    }

    readRadixNumber(radix) {
      let start = this.pos
      this.pos += 2 // 0x
      let val = this.readInt(radix)
      if (val === null) this.raise(this.start + 2, `Expected number in radix ${radix}`)
      if (this.input.charCodeAt(this.pos) == 110) {
        let str = this.getNumberInput(start, this.pos)
        val = typeof BigInt !== "undefined" ? BigInt(str) : null
        ++this.pos
      } else if (isIdentifierStart(this.fullCharCodeAtPos())) this.raise(this.pos, "Identifier directly after number")
      return this.finishToken(tt.num, val)
    }

    readNumber(startsWithDot) {
      let start = this.pos

      // Not an int
      if (startsWithDot) return super.readNumber(startsWithDot)

      // Legacy octal
      if (this.input.charCodeAt(start) === 48 && this.input.charCodeAt(start + 1) !== 110) {
        return super.readNumber(startsWithDot)
      }

      if (this.readInt(10) === null) this.raise(start, "Invalid number")

      // Not a BigInt, reset and parse again
      if (this.input.charCodeAt(this.pos) != 110) {
        this.pos = start
        return super.readNumber(startsWithDot)
      }

      let str = this.getNumberInput(start, this.pos)
      let val = typeof BigInt !== "undefined" ? BigInt(str) : null
      ++this.pos
      return this.finishToken(tt.num, val)
    }

    // This is basically a hook for acorn-numeric-separator
    getNumberInput(start, end) {
      if (super.getNumberInput) return super.getNumberInput(start, end)
      return this.input.slice(start, end)
    }
  }
}
