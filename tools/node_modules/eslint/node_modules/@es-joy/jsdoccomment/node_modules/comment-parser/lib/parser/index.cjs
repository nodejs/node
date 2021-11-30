"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

const util_1 = require("../util.cjs");

const block_parser_1 = require("./block-parser.cjs");

const source_parser_1 = require("./source-parser.cjs");

const spec_parser_1 = require("./spec-parser.cjs");

const tag_1 = require("./tokenizers/tag.cjs");

const type_1 = require("./tokenizers/type.cjs");

const name_1 = require("./tokenizers/name.cjs");

const description_1 = require("./tokenizers/description.cjs");

function getParser({
  startLine = 0,
  fence = '```',
  spacing = 'compact',
  tokenizers = [tag_1.default(), type_1.default(spacing), name_1.default(), description_1.default(spacing)]
} = {}) {
  if (startLine < 0 || startLine % 1 > 0) throw new Error('Invalid startLine');
  const parseSource = source_parser_1.default({
    startLine
  });
  const parseBlock = block_parser_1.default({
    fence
  });
  const parseSpec = spec_parser_1.default({
    tokenizers
  });
  const joinDescription = description_1.getJoiner(spacing);

  const notEmpty = line => line.tokens.description.trim() != '';

  return function (source) {
    const blocks = [];

    for (const line of util_1.splitLines(source)) {
      const lines = parseSource(line);
      if (lines === null) continue;
      if (lines.find(notEmpty) === undefined) continue;
      const sections = parseBlock(lines);
      const specs = sections.slice(1).map(parseSpec);
      blocks.push({
        description: joinDescription(sections[0]),
        tags: specs,
        source: lines,
        problems: specs.reduce((acc, spec) => acc.concat(spec.problems), [])
      });
    }

    return blocks;
  };
}

exports.default = getParser;
//# sourceMappingURL=index.cjs.map
