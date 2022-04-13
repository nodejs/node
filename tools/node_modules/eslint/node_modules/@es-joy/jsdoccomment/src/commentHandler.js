import esquery from 'esquery';

import {
  visitorKeys as jsdocTypePrattParserVisitorKeys
} from 'jsdoc-type-pratt-parser';

import {
  commentParserToESTree, jsdocVisitorKeys
} from './commentParserToESTree.js';

/**
 * @callback CommentHandler
 * @param {string} commentSelector
 * @param {Node} jsdoc
 * @returns {boolean}
 */

/**
 * @param {Settings} settings
 * @returns {CommentHandler}
 */
const commentHandler = (settings) => {
  /**
   * @type {CommentHandler}
   */
  return (commentSelector, jsdoc) => {
    const {mode} = settings;

    const selector = esquery.parse(commentSelector);

    const ast = commentParserToESTree(jsdoc, mode);

    return esquery.matches(ast, selector, null, {
      visitorKeys: {
        ...jsdocTypePrattParserVisitorKeys,
        ...jsdocVisitorKeys
      }
    });
  };
};

export default commentHandler;
