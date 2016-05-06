import {Parser} from "./state"
import {lineBreakG} from "./whitespace"
import {deprecate} from "util"

// These are used when `options.locations` is on, for the
// `startLoc` and `endLoc` properties.

export class Position {
  constructor(line, col) {
    this.line = line
    this.column = col
  }

  offset(n) {
    return new Position(this.line, this.column + n)
  }
}

export class SourceLocation {
  constructor(p, start, end) {
    this.start = start
    this.end = end
    if (p.sourceFile !== null) this.source = p.sourceFile
  }
}

// The `getLineInfo` function is mostly useful when the
// `locations` option is off (for performance reasons) and you
// want to find the line/column position for a given character
// offset. `input` should be the code string that the offset refers
// into.

export function getLineInfo(input, offset) {
  for (let line = 1, cur = 0;;) {
    lineBreakG.lastIndex = cur
    let match = lineBreakG.exec(input)
    if (match && match.index < offset) {
      ++line
      cur = match.index + match[0].length
    } else {
      return new Position(line, offset - cur)
    }
  }
}

const pp = Parser.prototype

// This function is used to raise exceptions on parse errors. It
// takes an offset integer (into the current `input`) to indicate
// the location of the error, attaches the position to the end
// of the error message, and then raises a `SyntaxError` with that
// message.

pp.raise = function(pos, message) {
  let loc = getLineInfo(this.input, pos)
  message += " (" + loc.line + ":" + loc.column + ")"
  let err = new SyntaxError(message)
  err.pos = pos; err.loc = loc; err.raisedAt = this.pos
  throw err
}

pp.curPosition = function() {
  return new Position(this.curLine, this.pos - this.lineStart)
}

pp.markPosition = function() {
  return this.options.locations ? [this.start, this.startLoc] : this.start
}
