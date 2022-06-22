export default function buildCSSForFlavoredJS(dynamicSizes) {
  if (dynamicSizes == null || dynamicSizes.length === 0) return '';

  return `<style>${Array.from(dynamicSizes, (charCount) =>
    `@media(max-width:${charCount * 8 + 142 + 16 * 4 + (charCount > 68 ? 274 : 0)}px){` +
      `.with-${charCount}-chars>.js-flavor-selector{` +
        'float:none;' +
        'margin:0 0 1em auto;' +
      '}' +
    '}').join('')}</style>`;
}
