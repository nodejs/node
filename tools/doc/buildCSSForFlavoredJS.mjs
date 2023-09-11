const CHAR_THRESHOLD = 68; // Around 68 characters, we have to take into
//                            account the left column that appears on screen
//                            wider than 1024px.

const ESTIMATED_CHAR_WIDTH = 8; // If the root element font-size is 16px (default value), 1ch is 7.8025px.
const TOGGLE_WIDTH = 142; // https://github.com/nodejs/node/blob/dbd938549b16718f2e4cc2809746b8c44a191b1d/doc/api_assets/style.css#L954

const PRE_MARGIN_LEFT = 16; // https://github.com/nodejs/node/blob/dbd938549b16718f2e4cc2809746b8c44a191b1d/doc/api_assets/style.css#L516
const PRE_MARGIN_RIGHT = 16; // https://github.com/nodejs/node/blob/dbd938549b16718f2e4cc2809746b8c44a191b1d/doc/api_assets/style.css#L516
const PRE_PADDING_LEFT = 16; // https://github.com/nodejs/node/blob/dbd938549b16718f2e4cc2809746b8c44a191b1d/doc/api_assets/style.css#L513
const PRE_PADDING_RIGHT = 16; // https://github.com/nodejs/node/blob/dbd938549b16718f2e4cc2809746b8c44a191b1d/doc/api_assets/style.css#L513


const COLUMN_RIGHT_MARGIN_LEFT_LARGE_SCREEN = 234; // https://github.com/nodejs/node/blob/dbd938549b16718f2e4cc2809746b8c44a191b1d/doc/api_assets/style.css#L653
const COLUMN_RIGHT_MARGIN_LEFT_SMALL_SCREEN = 0; // https://github.com/nodejs/node/blob/dbd938549b16718f2e4cc2809746b8c44a191b1d/doc/api_assets/style.css#L906
const COLUMN_RIGHT_MARGIN_RIGHT_LARGE_SCREEN = 0;
const COLUMN_RIGHT_MARGIN_RIGHT_SMALL_SCREEN = 0;
const COLUMN_RIGHT_PADDING_LEFT_LARGE_SCREEN = 24; // https://github.com/nodejs/node/blob/dbd938549b16718f2e4cc2809746b8c44a191b1d/doc/api_assets/style.css#L655
const COLUMN_RIGHT_PADDING_LEFT_SMALL_SCREEN = 8; // https://github.com/nodejs/node/blob/dbd938549b16718f2e4cc2809746b8c44a191b1d/doc/api_assets/style.css#L907
const COLUMN_RIGHT_PADDING_RIGHT_LARGE_SCREEN = 32; // https://github.com/nodejs/node/blob/dbd938549b16718f2e4cc2809746b8c44a191b1d/doc/api_assets/style.css#L654
const COLUMN_RIGHT_PADDING_RIGHT_SMALL_SCREEN = 8; // https://github.com/nodejs/node/blob/dbd938549b16718f2e4cc2809746b8c44a191b1d/doc/api_assets/style.css#L908

const getMarginLeft = (charCount) =>
  (charCount > CHAR_THRESHOLD ?
    COLUMN_RIGHT_MARGIN_LEFT_LARGE_SCREEN :
    COLUMN_RIGHT_MARGIN_LEFT_SMALL_SCREEN) + PRE_MARGIN_LEFT;
const getPaddingLeft = (charCount) =>
  (charCount > CHAR_THRESHOLD ?
    COLUMN_RIGHT_PADDING_LEFT_LARGE_SCREEN :
    COLUMN_RIGHT_PADDING_LEFT_SMALL_SCREEN) + PRE_PADDING_LEFT;
const getPaddingRight = (charCount) =>
  (charCount > CHAR_THRESHOLD ?
    COLUMN_RIGHT_PADDING_RIGHT_LARGE_SCREEN :
    COLUMN_RIGHT_PADDING_RIGHT_SMALL_SCREEN) + PRE_PADDING_RIGHT;
const getMarginRight = (charCount) =>
  (charCount > CHAR_THRESHOLD ?
    COLUMN_RIGHT_MARGIN_RIGHT_LARGE_SCREEN :
    COLUMN_RIGHT_MARGIN_RIGHT_SMALL_SCREEN) + PRE_MARGIN_RIGHT;


export default function buildCSSForFlavoredJS(dynamicSizes) {
  if (dynamicSizes == null || dynamicSizes.length === 0) return '';

  return `<style>${Array.from(dynamicSizes, (charCount) =>
    `@media(max-width:${getMarginLeft(charCount) + getPaddingLeft(charCount) +
                        charCount * ESTIMATED_CHAR_WIDTH + TOGGLE_WIDTH +
                        getPaddingRight(charCount) + getMarginRight(charCount)}px){` +
      `.with-${charCount}-chars>.js-flavor-toggle{` +
        'float:none;' +
        'margin:0 0 1em auto;' +
      '}' +
    '}').join('')}</style>`;
}
