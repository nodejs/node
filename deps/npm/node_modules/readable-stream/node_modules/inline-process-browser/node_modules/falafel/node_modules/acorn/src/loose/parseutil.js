import {LooseParser} from "./state"
import {Node, SourceLocation, lineBreak, isNewLine, tokTypes as tt} from ".."

const lp = LooseParser.prototype

lp.startNode = function() {
  let node = new Node
  node.start = this.tok.start
  if (this.options.locations)
    node.loc = new SourceLocation(this.toks, this.tok.loc.start)
  if (this.options.directSourceFile)
    node.sourceFile = this.options.directSourceFile
  if (this.options.ranges)
    node.range = [this.tok.start, 0]
  return node
}

lp.storeCurrentPos = function() {
  return this.options.locations ? [this.tok.start, this.tok.loc.start] : this.tok.start
}

lp.startNodeAt = function(pos) {
  let node = new Node
  if (this.options.locations) {
    node.start = pos[0]
    node.loc = new SourceLocation(this.toks, pos[1])
    pos = pos[0]
  } else {
    node.start = pos
  }
  if (this.options.directSourceFile)
    node.sourceFile = this.options.directSourceFile
  if (this.options.ranges)
    node.range = [pos, 0]
  return node
}

lp.finishNode = function(node, type) {
  node.type = type
  node.end = this.last.end
  if (this.options.locations)
    node.loc.end = this.last.loc.end
  if (this.options.ranges)
    node.range[1] = this.last.end
  return node
}

lp.dummyIdent = function() {
  let dummy = this.startNode()
  dummy.name = "✖"
  return this.finishNode(dummy, "Identifier")
}

export function isDummy(node) { return node.name == "✖" }

lp.eat = function(type) {
  if (this.tok.type === type) {
    this.next()
    return true
  } else {
    return false
  }
}

lp.isContextual = function(name) {
  return this.tok.type === tt.name && this.tok.value === name
}

lp.eatContextual = function(name) {
  return this.tok.value === name && this.eat(tt.name)
}

lp.canInsertSemicolon = function() {
  return this.tok.type === tt.eof || this.tok.type === tt.braceR ||
    lineBreak.test(this.input.slice(this.last.end, this.tok.start))
}

lp.semicolon = function() {
  return this.eat(tt.semi)
}

lp.expect = function(type) {
  if (this.eat(type)) return true
  for (let i = 1; i <= 2; i++) {
    if (this.lookAhead(i).type == type) {
      for (let j = 0; j < i; j++) this.next()
      return true
    }
  }
}

lp.pushCx = function() {
  this.context.push(this.curIndent)
}
lp.popCx = function() {
  this.curIndent = this.context.pop()
}

lp.lineEnd = function(pos) {
  while (pos < this.input.length && !isNewLine(this.input.charCodeAt(pos))) ++pos
  return pos
}

lp.indentationAfter = function(pos) {
  for (let count = 0;; ++pos) {
    let ch = this.input.charCodeAt(pos)
    if (ch === 32) ++count
    else if (ch === 9) count += this.options.tabSize
    else return count
  }
}

lp.closes = function(closeTok, indent, line, blockHeuristic) {
  if (this.tok.type === closeTok || this.tok.type === tt.eof) return true
  return line != this.curLineStart && this.curIndent < indent && this.tokenStartsLine() &&
    (!blockHeuristic || this.nextLineStart >= this.input.length ||
     this.indentationAfter(this.nextLineStart) < indent)
}

lp.tokenStartsLine = function() {
  for (let p = this.tok.start - 1; p >= this.curLineStart; --p) {
    let ch = this.input.charCodeAt(p)
    if (ch !== 9 && ch !== 32) return false
  }
  return true
}
