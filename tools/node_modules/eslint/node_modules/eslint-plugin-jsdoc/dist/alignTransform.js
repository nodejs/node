"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;

var _commentParser = require("comment-parser");

/**
 * Transform based on https://github.com/syavorsky/comment-parser/blob/master/src/transforms/align.ts
 *
 * It contains some customizations to align based on the tags, and some custom options.
 */
const {
  rewireSource
} = _commentParser.util;
const zeroWidth = {
  name: 0,
  start: 0,
  tag: 0,
  type: 0
};

const shouldAlign = (tags, index, source) => {
  const tag = source[index].tokens.tag.replace('@', '');
  const includesTag = tags.includes(tag);

  if (includesTag) {
    return true;
  }

  if (tag !== '') {
    return false;
  }

  for (let iterator = index; iterator >= 0; iterator--) {
    const previousTag = source[iterator].tokens.tag.replace('@', '');

    if (previousTag !== '') {
      if (tags.includes(previousTag)) {
        return true;
      }

      return false;
    }
  }

  return true;
};

const getWidth = tags => {
  return (width, {
    tokens
  }, index, source) => {
    if (!shouldAlign(tags, index, source)) {
      return width;
    }

    return {
      name: Math.max(width.name, tokens.name.length),
      start: tokens.delimiter === _commentParser.Markers.start ? tokens.start.length : width.start,
      tag: Math.max(width.tag, tokens.tag.length),
      type: Math.max(width.type, tokens.type.length)
    };
  };
};

const space = len => {
  return ''.padStart(len, ' ');
};

const alignTransform = ({
  customSpacings,
  tags,
  indent,
  preserveMainDescriptionPostDelimiter
}) => {
  let intoTags = false;
  let width;

  const alignTokens = tokens => {
    const nothingAfter = {
      delim: false,
      name: false,
      tag: false,
      type: false
    };

    if (tokens.description === '') {
      nothingAfter.name = true;
      tokens.postName = '';

      if (tokens.name === '') {
        nothingAfter.type = true;
        tokens.postType = '';

        if (tokens.type === '') {
          nothingAfter.tag = true;
          tokens.postTag = '';
          /* istanbul ignore next: Never happens because the !intoTags return. But it's here for consistency with the original align transform */

          if (tokens.tag === '') {
            nothingAfter.delim = true;
          }
        }
      }
    } // Todo: Avoid fixing alignment of blocks with multiline wrapping of type


    if (tokens.tag === '' && tokens.type) {
      return tokens;
    }

    const spacings = {
      postDelimiter: (customSpacings === null || customSpacings === void 0 ? void 0 : customSpacings.postDelimiter) || 1,
      postName: (customSpacings === null || customSpacings === void 0 ? void 0 : customSpacings.postName) || 1,
      postTag: (customSpacings === null || customSpacings === void 0 ? void 0 : customSpacings.postTag) || 1,
      postType: (customSpacings === null || customSpacings === void 0 ? void 0 : customSpacings.postType) || 1
    };
    tokens.postDelimiter = nothingAfter.delim ? '' : space(spacings.postDelimiter);

    if (!nothingAfter.tag) {
      tokens.postTag = space(width.tag - tokens.tag.length + spacings.postTag);
    }

    if (!nothingAfter.type) {
      tokens.postType = space(width.type - tokens.type.length + spacings.postType);
    }

    if (!nothingAfter.name) {
      // If post name is empty for all lines (name width 0), don't add post name spacing.
      tokens.postName = width.name === 0 ? '' : space(width.name - tokens.name.length + spacings.postName);
    }

    return tokens;
  };

  const update = (line, index, source) => {
    const tokens = { ...line.tokens
    };

    if (tokens.tag !== '') {
      intoTags = true;
    }

    const isEmpty = tokens.tag === '' && tokens.name === '' && tokens.type === '' && tokens.description === ''; // dangling '*/'

    if (tokens.end === _commentParser.Markers.end && isEmpty) {
      tokens.start = indent + ' ';
      return { ...line,
        tokens
      };
    }
    /* eslint-disable indent */


    switch (tokens.delimiter) {
      case _commentParser.Markers.start:
        tokens.start = indent;
        break;

      case _commentParser.Markers.delim:
        tokens.start = indent + ' ';
        break;

      default:
        tokens.delimiter = ''; // compensate delimiter

        tokens.start = indent + '  ';
    }
    /* eslint-enable */


    if (!intoTags) {
      if (tokens.description === '') {
        tokens.postDelimiter = '';
      } else if (!preserveMainDescriptionPostDelimiter) {
        tokens.postDelimiter = ' ';
      }

      return { ...line,
        tokens
      };
    } // Not align.


    if (!shouldAlign(tags, index, source)) {
      return { ...line,
        tokens
      };
    }

    return { ...line,
      tokens: alignTokens(tokens)
    };
  };

  return ({
    source,
    ...fields
  }) => {
    width = source.reduce(getWidth(tags), { ...zeroWidth
    });
    return rewireSource({ ...fields,
      source: source.map((line, index) => {
        return update(line, index, source);
      })
    });
  };
};

var _default = alignTransform;
exports.default = _default;
module.exports = exports.default;
//# sourceMappingURL=alignTransform.js.map