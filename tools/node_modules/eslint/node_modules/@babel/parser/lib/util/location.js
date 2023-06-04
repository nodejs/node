"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.SourceLocation = exports.Position = void 0;
exports.createPositionWithColumnOffset = createPositionWithColumnOffset;
class Position {
  constructor(line, col, index) {
    this.line = void 0;
    this.column = void 0;
    this.index = void 0;
    this.line = line;
    this.column = col;
    this.index = index;
  }
}
exports.Position = Position;
class SourceLocation {
  constructor(start, end) {
    this.start = void 0;
    this.end = void 0;
    this.filename = void 0;
    this.identifierName = void 0;
    this.start = start;
    this.end = end;
  }
}
exports.SourceLocation = SourceLocation;
function createPositionWithColumnOffset(position, columnOffset) {
  const {
    line,
    column,
    index
  } = position;
  return new Position(line, column + columnOffset, index + columnOffset);
}

//# sourceMappingURL=location.js.map
