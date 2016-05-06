import {Parser} from "./state"
import {SourceLocation} from "./location"

// Start an AST node, attaching a start offset.

const pp = Parser.prototype

export class Node {}

pp.startNode = function() {
  let node = new Node
  node.start = this.start
  if (this.options.locations)
    node.loc = new SourceLocation(this, this.startLoc)
  if (this.options.directSourceFile)
    node.sourceFile = this.options.directSourceFile
  if (this.options.ranges)
    node.range = [this.start, 0]
  return node
}

pp.startNodeAt = function(pos, loc) {
  let node = new Node
  if (Array.isArray(pos)){
    if (this.options.locations && loc === undefined) {
      // flatten pos
      loc = pos[1]
      pos = pos[0]
    }
  }
  node.start = pos
  if (this.options.locations)
    node.loc = new SourceLocation(this, loc)
  if (this.options.directSourceFile)
    node.sourceFile = this.options.directSourceFile
  if (this.options.ranges)
    node.range = [pos, 0]
  return node
}

// Finish an AST node, adding `type` and `end` properties.

pp.finishNode = function(node, type) {
  node.type = type
  node.end = this.lastTokEnd
  if (this.options.locations)
    node.loc.end = this.lastTokEndLoc
  if (this.options.ranges)
    node.range[1] = this.lastTokEnd
  return node
}

// Finish node at given position

pp.finishNodeAt = function(node, type, pos, loc) {
  node.type = type
  if (Array.isArray(pos)){
    if (this.options.locations && loc === undefined) {
      // flatten pos
      loc = pos[1]
      pos = pos[0]
    }
  }
  node.end = pos
  if (this.options.locations)
    node.loc.end = loc
  if (this.options.ranges)
    node.range[1] = pos
  return node
}
