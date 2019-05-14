"use strict"

module.exports = function(Parser) {
  return class extends Parser {
    readInt(radix, len) {
      // Hack: len is only != null for unicode escape sequences,
      // where numeric separators are not allowed
      if (len != null) return super.readInt(radix, len)

      let start = this.pos, total = 0, acceptUnderscore = false
      for (;;) {
        let code = this.input.charCodeAt(this.pos), val
        if (code >= 97) val = code - 97 + 10 // a
        else if (code == 95) {
          if (!acceptUnderscore) this.raise(this.pos, "Invalid numeric separator")
          ++this.pos
          acceptUnderscore = false
          continue
        } else if (code >= 65) val = code - 65 + 10 // A
        else if (code >= 48 && code <= 57) val = code - 48 // 0-9
        else val = Infinity
        if (val >= radix) break
        ++this.pos
        total = total * radix + val
        acceptUnderscore = true
      }
      if (this.pos === start) return null
      if (!acceptUnderscore) this.raise(this.pos - 1, "Invalid numeric separator")

      return total
    }

    readNumber(startsWithDot) {
      const token = super.readNumber(startsWithDot)
      let octal = this.end - this.start >= 2 && this.input.charCodeAt(this.start) === 48
      const stripped = this.getNumberInput(this.start, this.end)
      if (stripped.length < this.end - this.start) {
        if (octal) this.raise(this.start, "Invalid number")
        this.value = parseFloat(stripped)
      }
      return token
    }

    // This is used by acorn-bigint
    getNumberInput(start, end) {
      return this.input.slice(start, end).replace(/_/g, "")
    }
  }
}
