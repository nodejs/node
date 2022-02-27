import {parse as jsdocTypePrattParse} from 'jsdoc-type-pratt-parser';

/**
 * Removes initial and ending brackets from `rawType`
 * @param {JsdocTypeLine[]|JsdocTag} container
 * @param {boolean} isArr
 * @returns {void}
 */
const stripEncapsulatingBrackets = (container, isArr) => {
  if (isArr) {
    const firstItem = container[0];
    firstItem.rawType = firstItem.rawType.replace(
      /^\{/u, ''
    );

    const lastItem = container[container.length - 1];
    lastItem.rawType = lastItem.rawType.replace(/\}$/u, '');

    return;
  }
  container.rawType = container.rawType.replace(
    /^\{/u, ''
  ).replace(/\}$/u, '');
};

/**
 * Strips brackets from a tag's `rawType` values and adds `parsedType`
 * @param {JsdocTag} lastTag
 * @param {external:JsdocTypePrattParserMode} mode
 * @returns {void}
 */
const cleanUpLastTag = (lastTag, mode) => {
  // Strip out `}` that encapsulates and is not part of
  //   the type
  stripEncapsulatingBrackets(lastTag);
  if (lastTag.typeLines.length) {
    stripEncapsulatingBrackets(lastTag.typeLines, true);
  }

  // With even a multiline type now in full, add parsing
  let parsedType = null;

  try {
    parsedType = jsdocTypePrattParse(lastTag.rawType, mode);
  } catch (err) {
    // Ignore
  }

  lastTag.parsedType = parsedType;
};

/**
 * @external CommentParserJsdoc
 */

/**
 * @external JsdocTypePrattParserMode
 */

/**
 * @typedef {{
 *   delimiter: string,
 *   postDelimiter: string,
 *   rawType: string,
 *   start: string,
 *   type: "JsdocTypeLine"
 * }} JsdocTypeLine
 */

/**
 * @typedef {{
 *   delimiter: string,
 *   description: string,
 *   postDelimiter: string,
 *   start: string,
 *   type: "JsdocDescriptionLine"
 * }} JsdocDescriptionLine
 */

/**
 * @typedef {{
 *   delimiter: string,
 *   description: string,
 *   postDelimiter: string,
 *   start: string,
 *   tag: string,
 *   end: string,
 *   type: string,
 *   descriptionLines: JsdocDescriptionLine[],
 *   rawType: string,
 *   type: "JsdocTag",
 *   typeLines: JsdocTypeLine[]
 * }} JsdocTag
 */

/**
 * @typedef {{
 *   delimiter: string,
 *   description: string,
 *   descriptionLines: JsdocDescriptionLine[],
 *   end: string,
 *   postDelimiter: string,
 *   lineEnd: string,
 *   type: "JsdocBlock",
 *   lastDescriptionLine: Integer,
 *   tags: JsdocTag[]
 * }} JsdocBlock
 */

/**
 *
 * @param {external:CommentParserJsdoc} jsdoc
 * @param {external:JsdocTypePrattParserMode} mode
 * @returns {JsdocBlock}
 */
const commentParserToESTree = (jsdoc, mode) => {
  const {source} = jsdoc;

  const {tokens: {
    delimiter: delimiterRoot,
    lineEnd: lineEndRoot,
    postDelimiter: postDelimiterRoot,
    end: endRoot,
    description: descriptionRoot
  }} = source[0];

  const endLine = source.length - 1;
  const ast = {
    delimiter: delimiterRoot,
    description: descriptionRoot,

    descriptionLines: [],

    // `end` will be overwritten if there are other entries
    end: endRoot,
    endLine,
    postDelimiter: postDelimiterRoot,
    lineEnd: lineEndRoot,

    type: 'JsdocBlock'
  };

  const tags = [];
  let lastDescriptionLine;
  let lastTag = null;

  source.forEach((info, idx) => {
    const {tokens} = info;
    const {
      delimiter,
      description,
      postDelimiter,
      start,
      tag,
      end,
      type: rawType
    } = tokens;

    if (tag || end) {
      if (lastDescriptionLine === undefined) {
        lastDescriptionLine = idx;
      }

      // Clean-up with last tag before end or new tag
      if (lastTag) {
        cleanUpLastTag(lastTag, mode);
      }

      // Stop the iteration when we reach the end
      // but only when there is no tag earlier in the line
      // to still process
      if (end && !tag) {
        ast.end = end;

        return;
      }

      const {
        end: ed,
        delimiter: de,
        postDelimiter: pd,
        ...tkns
      } = tokens;

      if (!tokens.name) {
        let i = 0;
        while (source[idx + i]) {
          const {tokens: {
            name,
            postName
          }} = source[idx + i];
          if (name) {
            tkns.name = name;
            tkns.postName = postName;
            break;
          }
          i++;
        }
      }

      const tagObj = {
        ...tkns,
        postDelimiter: lastDescriptionLine ? pd : '',
        delimiter: lastDescriptionLine ? de : '',
        descriptionLines: [],
        rawType: '',
        type: 'JsdocTag',
        typeLines: []
      };
      tagObj.tag = tagObj.tag.replace(/^@/u, '');

      lastTag = tagObj;

      tags.push(tagObj);
    }

    if (rawType) {
      // Will strip rawType brackets after this tag
      lastTag.typeLines.push(
        lastTag.typeLines.length
          ? {
            delimiter,
            postDelimiter,
            rawType,
            start,
            type: 'JsdocTypeLine'
          }
          : {
            delimiter: '',
            postDelimiter: '',
            rawType,
            start: '',
            type: 'JsdocTypeLine'
          }
      );
      lastTag.rawType += lastTag.rawType ? '\n' + rawType : rawType;
    }

    if (description) {
      const holder = lastTag || ast;
      holder.descriptionLines.push(
        holder.descriptionLines.length
          ? {
            delimiter,
            description,
            postDelimiter,
            start,
            type: 'JsdocDescriptionLine'
          }
          : {
            delimiter: '',
            description,
            postDelimiter: '',
            start: '',
            type: 'JsdocDescriptionLine'
          }
      );
      holder.description += holder.description
        ? '\n' + description
        : description;
    }

    // Clean-up where last line itself has tag content
    if (end && tag) {
      ast.end = end;

      cleanUpLastTag(lastTag, mode);
    }
  });

  ast.lastDescriptionLine = lastDescriptionLine;
  ast.tags = tags;

  return ast;
};

const jsdocVisitorKeys = {
  JsdocBlock: ['descriptionLines', 'tags'],
  JsdocDescriptionLine: [],
  JsdocTypeLine: [],
  JsdocTag: ['parsedType', 'typeLines', 'descriptionLines']
};

export {commentParserToESTree, jsdocVisitorKeys};
