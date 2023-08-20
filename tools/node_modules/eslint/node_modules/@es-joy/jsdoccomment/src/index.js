/**
 * @typedef {import('./commentParserToESTree.js').JsdocInlineTagNoType & {
 *   start: number,
 *   end: number,
 * }} InlineTag
 */

/**
 * @typedef {import('comment-parser').Spec & {
 *   line?: import('./commentParserToESTree.js').Integer,
 *   inlineTags: (import('./commentParserToESTree.js').JsdocInlineTagNoType & {
 *     line?: import('./commentParserToESTree.js').Integer
 *   })[]
 * }} JsdocTagWithInline
 */

/**
 * Expands on comment-parser's `Block` interface.
 * @typedef {{
 *   description: string,
 *   source: import('comment-parser').Line[],
 *   problems: import('comment-parser').Problem[],
 *   tags: JsdocTagWithInline[],
 *   inlineTags: (import('./commentParserToESTree.js').JsdocInlineTagNoType & {
 *     line?: import('./commentParserToESTree.js').Integer
 *   })[]
 * }} JsdocBlockWithInline
 */

/**
 * @typedef {{preferRawType?: boolean}} ESTreeToStringOptions
 */

/**
 * @callback CommentHandler
 * @param {string} commentSelector
 * @param {import('./index.js').JsdocBlockWithInline} jsdoc
 * @returns {boolean}
 */

export {visitorKeys as jsdocTypeVisitorKeys} from 'jsdoc-type-pratt-parser';

export * from 'jsdoc-type-pratt-parser';

export {default as commentHandler} from './commentHandler.js';

export {default as toCamelCase} from './toCamelCase.js';

export * from './parseComment.js';

export * from './commentParserToESTree.js';

export * from './jsdoccomment.js';

export {default as estreeToString} from './estreeToString.js';
