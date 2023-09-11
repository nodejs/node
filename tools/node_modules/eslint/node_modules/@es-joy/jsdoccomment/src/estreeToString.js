import {
  visitorKeys as jsdocTypePrattParserVisitorKeys,
  stringify
} from 'jsdoc-type-pratt-parser';

import {jsdocVisitorKeys} from './commentParserToESTree.js';

/**
 * @typedef {import('./index.js').ESTreeToStringOptions} ESTreeToStringOptions
 */

const stringifiers = {
  /**
   * @param {import('./commentParserToESTree.js').JsdocBlock} node
   * @param {ESTreeToStringOptions} opts
   * @param {string[]} descriptionLines
   * @param {string[]} tags
   * @returns {string}
   */
  JsdocBlock ({
    delimiter, postDelimiter, lineEnd, initial, terminal, endLine
  }, opts, descriptionLines, tags) {
    const alreadyHasLine =
        (descriptionLines.length && !tags.length &&
          descriptionLines.at(-1)?.endsWith('\n')) ||
         (tags.length && tags.at(-1)?.endsWith('\n'));
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

  /**
   * @param {import('./commentParserToESTree.js').JsdocDescriptionLine} node
   * @returns {string}
   */
  JsdocDescriptionLine ({
    initial, delimiter, postDelimiter, description
  }) {
    return `${initial}${delimiter}${postDelimiter}${description}`;
  },

  /**
   * @param {import('./commentParserToESTree.js').JsdocTypeLine} node
   * @returns {string}
   */
  JsdocTypeLine ({
    initial, delimiter, postDelimiter, rawType
  }) {
    return `${initial}${delimiter}${postDelimiter}${rawType}`;
  },

  /**
   * @param {import('./commentParserToESTree.js').JsdocInlineTag} node
   */
  JsdocInlineTag ({format, namepathOrURL, tag, text}) {
    return format === 'pipe'
      ? `{@${tag} ${namepathOrURL}|${text}}`
      : format === 'plain'
        ? `{@${tag} ${namepathOrURL}}`
        : format === 'prefix'
          ? `[${text}]{@${tag} ${namepathOrURL}}`
          // "space"
          : `{@${tag} ${namepathOrURL} ${text}}`;
  },

  /**
   * @param {import('./commentParserToESTree.js').JsdocTag} node
   * @param {ESTreeToStringOptions} opts
   * @param {string} parsedType
   * @param {string[]} typeLines
   * @param {string[]} descriptionLines
   * @returns {string}
   */
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
 * @param {import('./commentParserToESTree.js').JsdocBlock|
 *   import('./commentParserToESTree.js').JsdocDescriptionLine|
 *   import('./commentParserToESTree.js').JsdocTypeLine|
 *   import('./commentParserToESTree.js').JsdocTag|
 *   import('./commentParserToESTree.js').JsdocInlineTag|
 *   import('jsdoc-type-pratt-parser').RootResult
 * } node
 * @param {ESTreeToStringOptions} opts
 * @throws {Error}
 * @returns {string}
 */
function estreeToString (node, opts = {}) {
  if (Object.prototype.hasOwnProperty.call(stringifiers, node.type)) {
    const childNodeOrArray = visitorKeys[node.type];

    const args = /** @type {(string[]|string|null)[]} */ (
      childNodeOrArray.map((key) => {
        // @ts-expect-error
        return Array.isArray(node[key])
          // @ts-expect-error
          ? node[key].map(
            (
              /**
               * @type {import('./commentParserToESTree.js').JsdocBlock|
               *   import('./commentParserToESTree.js').JsdocDescriptionLine|
               *   import('./commentParserToESTree.js').JsdocTypeLine|
               *   import('./commentParserToESTree.js').JsdocTag|
               *   import('./commentParserToESTree.js').JsdocInlineTag}
               */
              item
            ) => {
              return estreeToString(item, opts);
            }
          )
          // @ts-expect-error
          : (node[key] === undefined || node[key] === null
            ? null
            // @ts-expect-error
            : estreeToString(node[key], opts));
      })
    );
    return stringifiers[
      /**
       * @type {import('./commentParserToESTree.js').JsdocBlock|
       *   import('./commentParserToESTree.js').JsdocDescriptionLine|
       *   import('./commentParserToESTree.js').JsdocTypeLine|
       *   import('./commentParserToESTree.js').JsdocTag}
       */
      (node).type
    ](
      node,
      opts,
      // @ts-expect-error
      ...args
    );
  }

  // We use raw type instead but it is a key as other apps may wish to traverse
  if (node.type.startsWith('JsdocType')) {
    return opts.preferRawType
      ? ''
      : `{${stringify(
        /** @type {import('jsdoc-type-pratt-parser').RootResult} */ (
          node
        )
      )}}`;
  }

  throw new Error(`Unhandled node type: ${node.type}`);
}

export default estreeToString;
