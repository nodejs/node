/* eslint-disable node/no-unsupported-features/es-syntax */

/**
 * @param {RegExp} regex
 * @param {string} newFlags
 * @param {Integer} [newLastIndex=regex.lastIndex]
 * @returns {RegExp}
 */
export default function mixinRegex (
  regex, newFlags, newLastIndex = regex.lastIndex
) {
  newFlags = newFlags || '';
  regex = new RegExp(
    regex.source,
    (newFlags.includes('g') ? 'g' : regex.global ? 'g' : '') +
            (newFlags.includes('i') ? 'i' : regex.ignoreCase ? 'i' : '') +
            (newFlags.includes('m') ? 'm' : regex.multiline ? 'm' : '') +
            (newFlags.includes('u') ? 'u' : regex.unicode ? 'u' : '') +
            (newFlags.includes('y') ? 'y' : regex.sticky ? 'y' : '') +
            (newFlags.includes('s') ? 's' : regex.dotAll ? 's' : '')
  );
  regex.lastIndex = newLastIndex;
  return regex;
}
