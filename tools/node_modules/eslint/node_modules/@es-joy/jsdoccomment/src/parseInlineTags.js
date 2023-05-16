
/**
 * @param {RegExpMatchArray} match An inline tag regexp match.
 * @returns {string}
 */
function determineFormat (match) {
  const {separator, text} = match.groups;
  const [, textEnd] = match.indices.groups.text;
  const [tagStart] = match.indices.groups.tag;
  if (!text) {
    return 'plain';
  } else if (separator === '|') {
    return 'pipe';
  } else if (textEnd < tagStart) {
    return 'prefix';
  }
  return 'space';
}

/**
 * Extracts inline tags from a description.
 * @param {string} description
 * @returns {InlineTag[]} Array of inline tags from the description.
 */
function parseDescription (description) {
  const result = [];

  // This could have been expressed in a single pattern,
  // but having two avoids a potentially exponential time regex.

  // eslint-disable-next-line prefer-regex-literals -- Need 'd' (indices) flag
  const prefixedTextPattern = new RegExp(/(?:\[(?<text>[^\]]+)\])\{@(?<tag>[^}\s]+)\s?(?<namepathOrURL>[^}\s|]*)\}/gu, 'gud');
  // The pattern used to match for text after tag uses a negative lookbehind
  // on the ']' char to avoid matching the prefixed case too.
  // eslint-disable-next-line prefer-regex-literals -- Need 'd' (indices) flag
  const suffixedAfterPattern = new RegExp(/(?<!\])\{@(?<tag>[^}\s]+)\s?(?<namepathOrURL>[^}\s|]*)\s*(?<separator>[\s|])?\s*(?<text>[^}]*)\}/gu, 'gud');

  const matches = [
    ...description.matchAll(prefixedTextPattern),
    ...description.matchAll(suffixedAfterPattern)
  ];

  for (const match of matches) {
    const {tag, namepathOrURL, text} = match.groups;
    const [start, end] = match.indices[0];
    const format = determineFormat(match);

    result.push({
      tag,
      namepathOrURL,
      text,
      format,
      start,
      end
    });
  }

  return result;
}

/**
 * Splits the `{@prefix}` from remaining `Spec.lines[].token.description`
 * into the `inlineTags` tokens, and populates `spec.inlineTags`
 * @param {Block} block
 */
export default function parseInlineTags (block) {
  block.inlineTags = parseDescription(block.description);
  for (const tag of block.tags) {
    tag.inlineTags = parseDescription(tag.description);
  }
  return block;
}
