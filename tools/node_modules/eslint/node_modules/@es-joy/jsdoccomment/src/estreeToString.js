import {
  visitorKeys as jsdocTypePrattParserVisitorKeys
} from 'jsdoc-type-pratt-parser';

import {jsdocVisitorKeys} from './commentParserToESTree.js';

const stringifiers = {
  JsdocBlock ({
    delimiter, postDelimiter, lineEnd, end, endLine
  }, descriptionLines, tags) {
    return `${delimiter}${postDelimiter}${endLine
      ? `
`
      : ''}${
      // Could use `node.description` (and `node.lineEnd`), but lines may have
      //   been modified
      descriptionLines.length ? descriptionLines.join('') + lineEnd : ''
    }${
      tags.length ? tags.join('\n') + lineEnd : ''
    }${endLine
      ? `
 `
      : ''}${end}`;
  },
  JsdocDescriptionLine ({
    start, delimiter, postDelimiter, description
  }) {
    return `${start}${delimiter}${postDelimiter}${description}`;
  },
  JsdocTypeLine ({
    start, delimiter, postDelimiter, rawType
  }) {
    return `${delimiter}${postDelimiter}{${rawType}}`;
  },
  JsdocTag (node, parsedType, typeLines, descriptionLines) {
    const {
      description,
      name, postName, postTag, postType,
      start, delimiter, postDelimiter, tag
      // , rawType
    } = node;

    return `${start}${delimiter}${postDelimiter}@${tag}${postTag}${
      // Could do `rawType` but may have been changed; could also do
      //   `typeLines` but not as likely to be changed
      // parsedType
      // Comment this out later in favor of `parsedType`
      // We can't use raw `typeLines` as first argument has delimiter on it
      typeLines
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
 * @throws {Error}
 * @returns {string}
 */
function estreeToString (node) {
  if (Object.prototype.hasOwnProperty.call(stringifiers, node.type)) {
    const childNodeOrArray = visitorKeys[node.type];

    const args = childNodeOrArray.map((key) => {
      return Array.isArray(node[key])
        ? node[key].map((item) => {
          return estreeToString(item);
        })
        : (node[key] === undefined || node[key] === null
          ? []
          : [estreeToString(node[key])]);
    });

    return stringifiers[node.type](node, ...args);
  }

  // We use raw type instead but it is a key as other apps may wish to traverse
  if (node.type.startsWith('JsdocType')) {
    return '';
  }

  throw new Error(`Unhandled node type: ${node.type}`);
}

export default estreeToString;
