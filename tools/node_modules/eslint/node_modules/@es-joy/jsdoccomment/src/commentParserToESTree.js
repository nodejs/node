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
 *   initial: string,
 *   type: "JsdocTypeLine"
 * }} JsdocTypeLine
 */

/**
 * @typedef {{
 *   delimiter: string,
 *   description: string,
 *   postDelimiter: string,
 *   initial: string,
 *   type: "JsdocDescriptionLine"
 * }} JsdocDescriptionLine
 */

/**
 * @typedef {{
 *   delimiter: string,
 *   description: string,
 *   postDelimiter: string,
 *   initial: string,
 *   tag: string,
 *   terminal: string,
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
 *   initial: string,
 *   terminal: string,
 *   postDelimiter: string,
 *   lineEnd: string,
 *   type: "JsdocBlock",
 *   lastDescriptionLine: Integer,
 *   tags: JsdocTag[]
 * }} JsdocBlock
 */

/**
 * Converts comment parser AST to ESTree format.
 * @param {external:CommentParserJsdoc} jsdoc
 * @param {external:JsdocTypePrattParserMode} mode
 * @param {PlainObject} opts
 * @param {throwOnTypeParsingErrors} [opts.throwOnTypeParsingErrors=false]
 * @returns {JsdocBlock}
 */
const commentParserToESTree = (jsdoc, mode, {
  throwOnTypeParsingErrors = false
} = {}) => {
  /**
   * Strips brackets from a tag's `rawType` values and adds `parsedType`
   * @param {JsdocTag} lastTag
   * @returns {void}
   */
  const cleanUpLastTag = (lastTag) => {
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
      if (lastTag.rawType && throwOnTypeParsingErrors) {
        err.message = `Tag @${lastTag.tag} with raw type ` +
          `\`${lastTag.rawType}\` had parsing error: ${err.message}`;
        throw err;
      }
    }

    lastTag.parsedType = parsedType;
  };

  const {source} = jsdoc;

  const {tokens: {
    delimiter: delimiterRoot,
    lineEnd: lineEndRoot,
    postDelimiter: postDelimiterRoot,
    start: startRoot,
    end: endRoot
  }} = source[0];

  const endLine = source.length - 1;
  const ast = {
    delimiter: delimiterRoot,
    description: '',

    descriptionLines: [],

    initial: startRoot,
    // `terminal` will be overwritten if there are other entries
    terminal: endRoot,
    hasPreterminalDescription: 0,
    endLine,
    postDelimiter: postDelimiterRoot,
    lineEnd: lineEndRoot,

    type: 'JsdocBlock'
  };

  const tags = [];
  let lastDescriptionLine;
  let lastTag = null;
  let descLineStateOpen = true;

  source.forEach((info, idx) => {
    const {tokens} = info;
    const {
      delimiter,
      description,
      postDelimiter,
      start: initial,
      tag,
      end,
      type: rawType
    } = tokens;

    if (!tag && description && descLineStateOpen) {
      if (ast.descriptionStartLine === undefined) {
        ast.descriptionStartLine = idx;
      }
      ast.descriptionEndLine = idx;
    }

    if (tag || end) {
      descLineStateOpen = false;
      if (lastDescriptionLine === undefined) {
        lastDescriptionLine = idx;
      }

      // Clean-up with last tag before end or new tag
      if (lastTag) {
        cleanUpLastTag(lastTag);
      }

      // Stop the iteration when we reach the end
      // but only when there is no tag earlier in the line
      // to still process
      if (end && !tag) {
        ast.terminal = end;
        if (description) {
          if (lastTag) {
            ast.hasPreterminalTagDescription = 1;
          } else {
            ast.hasPreterminalDescription = 1;
          }

          const holder = lastTag || ast;
          holder.description += (holder.description ? '\n' : '') + description;
          holder.descriptionLines.push({
            delimiter,
            description,
            postDelimiter,
            initial,
            type: 'JsdocDescriptionLine'
          });
        }
        return;
      }

      const {
        end: ed,
        delimiter: de,
        postDelimiter: pd,
        start: init,
        ...tkns
      } = tokens;

      if (!tokens.name) {
        let i = 1;
        while (source[idx + i]) {
          const {tokens: {
            name,
            postName,
            postType,
            tag: tg
          }} = source[idx + i];
          if (tg) {
            break;
          }
          if (name) {
            tkns.postType = postType;
            tkns.name = name;
            tkns.postName = postName;
            break;
          }
          i++;
        }
      }

      const tagObj = {
        ...tkns,
        initial: endLine ? init : '',
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
            initial,
            type: 'JsdocTypeLine'
          }
          : {
            delimiter: '',
            postDelimiter: '',
            rawType,
            initial: '',
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
            initial,
            type: 'JsdocDescriptionLine'
          }
          : lastTag
            ? {
              delimiter: '',
              description,
              postDelimiter: '',
              initial: '',
              type: 'JsdocDescriptionLine'
            }
            : {
              delimiter,
              description,
              postDelimiter,
              initial,
              type: 'JsdocDescriptionLine'
            }
      );

      if (!tag) {
        holder.description += (!holder.description && !lastTag)
          ? description
          : '\n' + description;
      }
    }

    // Clean-up where last line itself has tag content
    if (end && tag) {
      ast.terminal = end;
      ast.hasPreterminalTagDescription = 1;

      cleanUpLastTag(lastTag);
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
