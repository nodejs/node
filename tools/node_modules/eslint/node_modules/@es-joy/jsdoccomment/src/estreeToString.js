import {
  visitorKeys as jsdocTypePrattParserVisitorKeys,
  stringify
} from 'jsdoc-type-pratt-parser';

import {jsdocVisitorKeys} from './commentParserToESTree.js';

const stringifiers = {
  JsdocBlock ({
    delimiter, postDelimiter, lineEnd, initial, terminal, endLine
  }, opts, descriptionLines, tags) {
    const alreadyHasLine =
        (descriptionLines.length && !tags.length &&
          descriptionLines[descriptionLines.length - 1].endsWith('\n')) ||
         (tags.length && tags[tags.length - 1].endsWith('\n'));
    return `${initial}${delimiter}${postDelimiter}${endLine
      ? `
`
      : ''}${
      // Could use `node.description` (and `node.lineEnd`), but lines may have
      //   been modified
      descriptionLines.length
        ? descriptionLines.join(
          lineEnd + '\n'
        ) + (tags.length ? lineEnd + '\n' : '')
        : ''
    }${
      tags.length ? tags.join(lineEnd + '\n') : ''
    }${endLine && !alreadyHasLine
      ? `${lineEnd}
 ${initial}`
      : endLine ? ` ${initial}` : ''}${terminal}`;
  },
  JsdocDescriptionLine ({
    initial, delimiter, postDelimiter, description
  }) {
    return `${initial}${delimiter}${postDelimiter}${description}`;
  },
  JsdocTypeLine ({
    initial, delimiter, postDelimiter, rawType, parsedType
  }) {
    return `${initial}${delimiter}${postDelimiter}${rawType}`;
  },
  JsdocTag (node, opts, parsedType, typeLines, descriptionLines) {
    const {
      description,
      name, postName, postTag, postType,
      initial, delimiter, postDelimiter, tag
      // , rawType
    } = node;

    return `${initial}${delimiter}${postDelimiter}@${tag}${postTag}${
      // Could do `rawType` but may have been changed; could also do
      //   `typeLines` but not as likely to be changed
      // parsedType
      // Comment this out later in favor of `parsedType`
      // We can't use raw `typeLines` as first argument has delimiter on it
      (opts.preferRawType || !parsedType)
        ? typeLines.length ? `{${typeLines.join('\n')}}` : ''
        : parsedType
    }${postType}${
      name ? `${name}${postName || (description ? '\n' : '')}` : ''
    }${descriptionLines.join('\n')}`;
  }
};

const visitorKeys = {...jsdocVisitorKeys, ...jsdocTypePrattParserVisitorKeys};

/**
 * @todo convert for use by escodegen (until may be patched to support
 *   custom entries?).
 * @param {Node} node
 * @param {{preferRawType: boolean}} opts
 * @throws {Error}
 * @returns {string}
 */
function estreeToString (node, opts = {}) {
  if (Object.prototype.hasOwnProperty.call(stringifiers, node.type)) {
    const childNodeOrArray = visitorKeys[node.type];

    const args = childNodeOrArray.map((key) => {
      return Array.isArray(node[key])
        ? node[key].map((item) => {
          return estreeToString(item, opts);
        })
        : (node[key] === undefined || node[key] === null
          ? null
          : estreeToString(node[key], opts));
    });
    return stringifiers[node.type](node, opts, ...args);
  }

  // We use raw type instead but it is a key as other apps may wish to traverse
  if (node.type.startsWith('JsdocType')) {
    return opts.preferRawType ? '' : `{${stringify(node)}}`;
  }

  throw new Error(`Unhandled node type: ${node.type}`);
}

export default estreeToString;
