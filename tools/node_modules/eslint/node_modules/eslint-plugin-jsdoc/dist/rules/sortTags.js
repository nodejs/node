"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = void 0;
var _defaultTagOrder = _interopRequireDefault(require("../defaultTagOrder"));
var _iterateJsdoc = _interopRequireDefault(require("../iterateJsdoc"));
function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }
var _default = (0, _iterateJsdoc.default)(({
  context,
  jsdoc,
  utils
}) => {
  const {
    tagSequence = _defaultTagOrder.default,
    alphabetizeExtras = false
  } = context.options[0] || {};
  const otherPos = tagSequence.indexOf('-other');
  const endPos = otherPos > -1 ? otherPos : tagSequence.length;
  let ongoingCount = 0;
  for (const [idx, tag] of jsdoc.tags.entries()) {
    tag.originalIndex = idx;
    ongoingCount += tag.source.length;
    tag.originalLine = ongoingCount;
  }
  let firstChangedTagLine;
  let firstChangedTagIndex;
  const sortedTags = JSON.parse(JSON.stringify(jsdoc.tags));
  sortedTags.sort(({
    tag: tagNew
  }, {
    originalIndex,
    originalLine,
    tag: tagOld
  }) => {
    // Optimize: Just keep relative positions if the same tag name
    if (tagNew === tagOld) {
      return 0;
    }
    const checkOrSetFirstChanged = () => {
      if (!firstChangedTagLine || originalLine < firstChangedTagLine) {
        firstChangedTagLine = originalLine;
        firstChangedTagIndex = originalIndex;
      }
    };
    const newPos = tagSequence.indexOf(tagNew);
    const oldPos = tagSequence.indexOf(tagOld);
    const preferredNewPos = newPos === -1 ? endPos : newPos;
    const preferredOldPos = oldPos === -1 ? endPos : oldPos;
    if (preferredNewPos < preferredOldPos) {
      checkOrSetFirstChanged();
      return -1;
    }
    if (preferredNewPos > preferredOldPos) {
      return 1;
    }

    // preferredNewPos === preferredOldPos
    if (!alphabetizeExtras ||
    // Optimize: If tagNew (or tagOld which is the same) was found in the
    //   priority array, it can maintain its relative position—without need
    //   of alphabetizing (secondary sorting)
    newPos >= 0) {
      return 0;
    }
    if (tagNew < tagOld) {
      checkOrSetFirstChanged();
      return -1;
    }

    // tagNew > tagOld
    return 1;
  });
  if (firstChangedTagLine === undefined) {
    return;
  }
  const firstLine = utils.getFirstLine();
  const fix = () => {
    const itemsToMoveRange = [...Array.from({
      length: jsdoc.tags.length - firstChangedTagIndex
    }).keys()];
    const unchangedPriorTagDescriptions = jsdoc.tags.slice(0, firstChangedTagIndex).reduce((ct, {
      source
    }) => {
      return ct + source.length - 1;
    }, 0);

    // This offset includes not only the offset from where the first tag
    //   must begin, and the additional offset of where the first changed
    //   tag begins, but it must also account for prior descriptions
    const initialOffset = firstLine + firstChangedTagIndex +
    // May be the first tag, so don't try finding a prior one if so
    unchangedPriorTagDescriptions;

    // Use `firstChangedTagLine` for line number to begin reporting/splicing
    for (const idx of itemsToMoveRange) {
      utils.removeTag(idx + firstChangedTagIndex);
    }
    const changedTags = sortedTags.slice(firstChangedTagIndex);
    let extraTagCount = 0;
    for (const idx of itemsToMoveRange) {
      const changedTag = changedTags[idx];
      utils.addTag(changedTag.tag, extraTagCount + initialOffset + idx, {
        ...changedTag.source[0].tokens,
        // `comment-parser` puts the `end` within the `tags` section, so
        //   avoid adding another to jsdoc.source
        end: ''
      });
      for (const {
        tokens
      } of changedTag.source.slice(1)) {
        if (!tokens.end) {
          utils.addLine(extraTagCount + initialOffset + idx + 1, {
            ...tokens,
            end: ''
          });
          extraTagCount++;
        }
      }
    }
  };
  utils.reportJSDoc(`Tags are not in the prescribed order: ${tagSequence.join(', ')}`, jsdoc.tags[firstChangedTagIndex], fix, true);
}, {
  iterateAllJsdocs: true,
  meta: {
    docs: {
      description: '',
      url: 'https://github.com/gajus/eslint-plugin-jsdoc#eslint-plugin-jsdoc-rules-sort-tags'
    },
    fixable: 'code',
    schema: [{
      additionalProperies: false,
      properties: {
        alphabetizeExtras: {
          type: 'boolean'
        },
        tagSequence: {
          items: {
            type: 'string'
          },
          type: 'array'
        }
      },
      type: 'object'
    }],
    type: 'suggestion'
  }
});
exports.default = _default;
module.exports = exports.default;
//# sourceMappingURL=sortTags.js.map