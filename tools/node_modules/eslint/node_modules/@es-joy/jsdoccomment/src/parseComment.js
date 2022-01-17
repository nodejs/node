/* eslint-disable prefer-named-capture-group -- Temporary */
import {
  parse as commentParser,
  tokenizers,
  util
} from 'comment-parser';

const {
  seedBlock,
  seedTokens
} = util;

const {
  name: nameTokenizer,
  tag: tagTokenizer,
  type: typeTokenizer,
  description: descriptionTokenizer
} = tokenizers;

export const hasSeeWithLink = (spec) => {
  return spec.tag === 'see' && (/\{@link.+?\}/u).test(spec.source[0].source);
};

export const defaultNoTypes = ['default', 'defaultvalue', 'see'];

export const defaultNoNames = [
  'access', 'author',
  'default', 'defaultvalue',
  'description',
  'example', 'exception',
  'license',
  'return', 'returns',
  'since', 'summary',
  'throws',
  'version', 'variation'
];

const getTokenizers = ({
  noTypes = defaultNoTypes,
  noNames = defaultNoNames
} = {}) => {
  // trim
  return [
    // Tag
    tagTokenizer(),

    // Type
    (spec) => {
      if (noTypes.includes(spec.tag)) {
        return spec;
      }

      return typeTokenizer()(spec);
    },

    // Name
    (spec) => {
      if (spec.tag === 'template') {
        // const preWS = spec.postTag;
        const remainder = spec.source[0].tokens.description;

        const pos = remainder.search(/(?<![\s,])\s/u);

        const name = pos === -1 ? remainder : remainder.slice(0, pos);
        const extra = remainder.slice(pos + 1);
        let postName = '', description = '', lineEnd = '';
        if (pos > -1) {
          [, postName, description, lineEnd] = extra.match(/(\s*)([^\r]*)(\r)?/u);
        }

        spec.name = name;
        spec.optional = false;
        const {tokens} = spec.source[0];
        tokens.name = name;
        tokens.postName = postName;
        tokens.description = description;
        tokens.lineEnd = lineEnd || '';

        return spec;
      }

      if (noNames.includes(spec.tag) || hasSeeWithLink(spec)) {
        return spec;
      }

      return nameTokenizer()(spec);
    },

    // Description
    (spec) => {
      return descriptionTokenizer('preserve')(spec);
    }
  ];
};

/**
 *
 * @param {PlainObject} commentNode
 * @param {string} indent Whitespace
 * @returns {PlainObject}
 */
const parseComment = (commentNode, indent) => {
  // Preserve JSDoc block start/end indentation.
  return commentParser(`/*${commentNode.value}*/`, {
    // @see https://github.com/yavorskiy/comment-parser/issues/21
    tokenizers: getTokenizers()
  })[0] || seedBlock({
    source: [
      {
        number: 0,
        tokens: seedTokens({
          delimiter: '/**'
        })
      },
      {
        number: 1,
        tokens: seedTokens({
          end: '*/',
          start: indent + ' '
        })
      }
    ]
  });
};

export {getTokenizers, parseComment};
