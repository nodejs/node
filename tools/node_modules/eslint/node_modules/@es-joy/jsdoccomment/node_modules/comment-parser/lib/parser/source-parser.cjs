"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

const primitives_1 = require("../primitives.cjs");

const util_1 = require("../util.cjs");

function getParser({
  startLine = 0
} = {}) {
  let block = null;
  let num = startLine;
  return function parseSource(source) {
    let rest = source;
    const tokens = util_1.seedTokens();
    [tokens.lineEnd, rest] = util_1.splitCR(rest);
    [tokens.start, rest] = util_1.splitSpace(rest);

    if (block === null && rest.startsWith(primitives_1.Markers.start) && !rest.startsWith(primitives_1.Markers.nostart)) {
      block = [];
      tokens.delimiter = rest.slice(0, primitives_1.Markers.start.length);
      rest = rest.slice(primitives_1.Markers.start.length);
      [tokens.postDelimiter, rest] = util_1.splitSpace(rest);
    }

    if (block === null) {
      num++;
      return null;
    }

    const isClosed = rest.trimRight().endsWith(primitives_1.Markers.end);

    if (tokens.delimiter === '' && rest.startsWith(primitives_1.Markers.delim) && !rest.startsWith(primitives_1.Markers.end)) {
      tokens.delimiter = primitives_1.Markers.delim;
      rest = rest.slice(primitives_1.Markers.delim.length);
      [tokens.postDelimiter, rest] = util_1.splitSpace(rest);
    }

    if (isClosed) {
      const trimmed = rest.trimRight();
      tokens.end = rest.slice(trimmed.length - primitives_1.Markers.end.length);
      rest = trimmed.slice(0, -primitives_1.Markers.end.length);
    }

    tokens.description = rest;
    block.push({
      number: num,
      source,
      tokens
    });
    num++;

    if (isClosed) {
      const result = block.slice();
      block = null;
      return result;
    }

    return null;
  };
}

exports.default = getParser;
//# sourceMappingURL=source-parser.cjs.map
