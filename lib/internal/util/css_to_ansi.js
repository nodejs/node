'use strict';

const {
  ArrayPrototypePush,
  MathMax,
  MathMin,
  MathRound,
  NumberIsNaN,
  NumberParseFloat,
  RegExpPrototypeExec,
  SafeMap,
  StringPrototypeCharCodeAt,
  StringPrototypeEndsWith,
  StringPrototypeIndexOf,
  StringPrototypeSlice,
  StringPrototypeSplit,
  StringPrototypeToLowerCase,
  StringPrototypeTrim,
} = primordials;

const kBasicColors = new SafeMap([
  ['black', 0],
  ['red', 1],
  ['green', 2],
  ['yellow', 3],
  ['blue', 4],
  ['magenta', 5],
  ['cyan', 6],
  ['white', 7],
]);

const kBasicColorRGB = [
  [0, 0, 0], [255, 0, 0], [0, 128, 0], [255, 255, 0],
  [0, 0, 255], [255, 0, 255], [0, 255, 255], [255, 255, 255],
];

// https://www.w3.org/TR/css-color-4/#named-colors
const kNamedColors = new SafeMap([
  ['aliceblue', 0xf0f8ff],
  ['antiquewhite', 0xfaebd7],
  ['aqua', 0x00ffff],
  ['aquamarine', 0x7fffd4],
  ['azure', 0xf0ffff],
  ['beige', 0xf5f5dc],
  ['bisque', 0xffe4c4],
  ['blanchedalmond', 0xffebcd],
  ['blueviolet', 0x8a2be2],
  ['brown', 0xa52a2a],
  ['burlywood', 0xdeb887],
  ['cadetblue', 0x5f9ea0],
  ['chartreuse', 0x7fff00],
  ['chocolate', 0xd2691e],
  ['coral', 0xff7f50],
  ['cornflowerblue', 0x6495ed],
  ['cornsilk', 0xfff8dc],
  ['crimson', 0xdc143c],
  ['darkblue', 0x00008b],
  ['darkcyan', 0x008b8b],
  ['darkgoldenrod', 0xb8860b],
  ['darkgray', 0xa9a9a9],
  ['darkgreen', 0x006400],
  ['darkgrey', 0xa9a9a9],
  ['darkkhaki', 0xbdb76b],
  ['darkmagenta', 0x8b008b],
  ['darkolivegreen', 0x556b2f],
  ['darkorange', 0xff8c00],
  ['darkorchid', 0x9932cc],
  ['darkred', 0x8b0000],
  ['darksalmon', 0xe9967a],
  ['darkseagreen', 0x8fbc8f],
  ['darkslateblue', 0x483d8b],
  ['darkslategray', 0x2f4f4f],
  ['darkslategrey', 0x2f4f4f],
  ['darkturquoise', 0x00ced1],
  ['darkviolet', 0x9400d3],
  ['deeppink', 0xff1493],
  ['deepskyblue', 0x00bfff],
  ['dimgray', 0x696969],
  ['dimgrey', 0x696969],
  ['dodgerblue', 0x1e90ff],
  ['firebrick', 0xb22222],
  ['floralwhite', 0xfffaf0],
  ['forestgreen', 0x228b22],
  ['fuchsia', 0xff00ff],
  ['gainsboro', 0xdcdcdc],
  ['ghostwhite', 0xf8f8ff],
  ['gold', 0xffd700],
  ['goldenrod', 0xdaa520],
  ['gray', 0x808080],
  ['greenyellow', 0xadff2f],
  ['grey', 0x808080],
  ['honeydew', 0xf0fff0],
  ['hotpink', 0xff69b4],
  ['indianred', 0xcd5c5c],
  ['indigo', 0x4b0082],
  ['ivory', 0xfffff0],
  ['khaki', 0xf0e68c],
  ['lavender', 0xe6e6fa],
  ['lavenderblush', 0xfff0f5],
  ['lawngreen', 0x7cfc00],
  ['lemonchiffon', 0xfffacd],
  ['lightblue', 0xadd8e6],
  ['lightcoral', 0xf08080],
  ['lightcyan', 0xe0ffff],
  ['lightgoldenrodyellow', 0xfafad2],
  ['lightgray', 0xd3d3d3],
  ['lightgreen', 0x90ee90],
  ['lightgrey', 0xd3d3d3],
  ['lightpink', 0xffb6c1],
  ['lightsalmon', 0xffa07a],
  ['lightseagreen', 0x20b2aa],
  ['lightskyblue', 0x87cefa],
  ['lightslategray', 0x778899],
  ['lightslategrey', 0x778899],
  ['lightsteelblue', 0xb0c4de],
  ['lightyellow', 0xffffe0],
  ['lime', 0x00ff00],
  ['limegreen', 0x32cd32],
  ['linen', 0xfaf0e6],
  ['maroon', 0x800000],
  ['mediumaquamarine', 0x66cdaa],
  ['mediumblue', 0x0000cd],
  ['mediumorchid', 0xba55d3],
  ['mediumpurple', 0x9370db],
  ['mediumseagreen', 0x3cb371],
  ['mediumslateblue', 0x7b68ee],
  ['mediumspringgreen', 0x00fa9a],
  ['mediumturquoise', 0x48d1cc],
  ['mediumvioletred', 0xc71585],
  ['midnightblue', 0x191970],
  ['mintcream', 0xf5fffa],
  ['mistyrose', 0xffe4e1],
  ['moccasin', 0xffe4b5],
  ['navajowhite', 0xffdead],
  ['navy', 0x000080],
  ['oldlace', 0xfdf5e6],
  ['olive', 0x808000],
  ['olivedrab', 0x6b8e23],
  ['orange', 0xffa500],
  ['orangered', 0xff4500],
  ['orchid', 0xda70d6],
  ['palegoldenrod', 0xeee8aa],
  ['palegreen', 0x98fb98],
  ['paleturquoise', 0xafeeee],
  ['palevioletred', 0xdb7093],
  ['papayawhip', 0xffefd5],
  ['peachpuff', 0xffdab9],
  ['peru', 0xcd853f],
  ['pink', 0xffc0cb],
  ['plum', 0xdda0dd],
  ['powderblue', 0xb0e0e6],
  ['purple', 0x800080],
  ['rebeccapurple', 0x663399],
  ['rosybrown', 0xbc8f8f],
  ['royalblue', 0x4169e1],
  ['saddlebrown', 0x8b4513],
  ['salmon', 0xfa8072],
  ['sandybrown', 0xf4a460],
  ['seagreen', 0x2e8b57],
  ['seashell', 0xfff5ee],
  ['sienna', 0xa0522d],
  ['silver', 0xc0c0c0],
  ['skyblue', 0x87ceeb],
  ['slateblue', 0x6a5acd],
  ['slategray', 0x708090],
  ['slategrey', 0x708090],
  ['snow', 0xfffafa],
  ['springgreen', 0x00ff7f],
  ['steelblue', 0x4682b4],
  ['tan', 0xd2b48c],
  ['teal', 0x008080],
  ['thistle', 0xd8bfd8],
  ['tomato', 0xff6347],
  ['turquoise', 0x40e0d0],
  ['violet', 0xee82ee],
  ['wheat', 0xf5deb3],
  ['whitesmoke', 0xf5f5f5],
  ['yellowgreen', 0x9acd32],
]);

const kHexColorRegExp = /^#([\da-f]{3,4}|[\da-f]{6}|[\da-f]{8})$/;
const kFunctionalColorRegExp = /^(rgba?|hsla?)\(([^()]*)\)$/;
const kNumberRegExp = /^[+-]?(?:\d+\.?\d*|\.\d+)(?:e[+-]?\d+)?$/;

function clampByte(value) {
  return MathRound(MathMax(0, MathMin(255, value)));
}

function parseHex(str) {
  const len = str.length;
  let value = 0;
  for (let i = 0; i < len; i++) {
    const code = StringPrototypeCharCodeAt(str, i);
    value = value * 16 + (code <= 57 ? code - 48 : code - 87);
  }
  return value;
}

function parseHexColor(hex) {
  const value = parseHex(hex);
  switch (hex.length) {
    case 3: // #rgb
      return [
        ((value >> 8) & 0xf) * 0x11,
        ((value >> 4) & 0xf) * 0x11,
        (value & 0xf) * 0x11,
      ];
    case 4: // #rgba
      return [
        ((value >> 12) & 0xf) * 0x11,
        ((value >> 8) & 0xf) * 0x11,
        ((value >> 4) & 0xf) * 0x11,
      ];
    case 6: // #rrggbb
      return [(value >> 16) & 0xff, (value >> 8) & 0xff, value & 0xff];
    default: // #rrggbbaa
      return [(value >> 24) & 0xff, (value >> 16) & 0xff, (value >> 8) & 0xff];
  }
}

// Parses one numeric component of a functional color notation.
function parseComponent(str, max, allowUnit) {
  let factor = 1;
  if (StringPrototypeEndsWith(str, '%')) {
    str = StringPrototypeSlice(str, 0, -1);
    factor = max / 100;
  } else if (allowUnit && StringPrototypeEndsWith(str, 'deg')) {
    str = StringPrototypeSlice(str, 0, -3);
  }
  if (RegExpPrototypeExec(kNumberRegExp, str) === null) {
    return NaN;
  }
  return NumberParseFloat(str) * factor;
}

// Splits the arguments of `rgb()` / `hsl()`
function splitComponents(str) {
  const parts = [];
  let current = '';
  for (let i = 0; i < str.length; i++) {
    const char = str[i];
    if (char === ',' || char === '/' || char === ' ' || char === '\t' ||
        char === '\n' || char === '\r' || char === '\f') {
      if (current !== '') {
        ArrayPrototypePush(parts, current);
        current = '';
      }
    } else {
      current += char;
    }
  }
  if (current !== '') {
    ArrayPrototypePush(parts, current);
  }
  return parts;
}

function hslToRgb(h, s, l) {
  h = (((h % 360) + 360) % 360) / 360;
  s = MathMax(0, MathMin(1, s));
  l = MathMax(0, MathMin(1, l));
  if (s === 0) {
    const gray = clampByte(l * 255);
    return [gray, gray, gray];
  }
  const q = l < 0.5 ? l * (1 + s) : l + s - l * s;
  const p = 2 * l - q;
  const channel = (t) => {
    if (t < 0) t += 1;
    if (t > 1) t -= 1;
    if (t < 1 / 6) return p + (q - p) * 6 * t;
    if (t < 1 / 2) return q;
    if (t < 2 / 3) return p + (q - p) * (2 / 3 - t) * 6;
    return p;
  };
  return [
    clampByte(channel(h + 1 / 3) * 255),
    clampByte(channel(h) * 255),
    clampByte(channel(h - 1 / 3) * 255),
  ];
}

// Parses a CSS color into either a basic palette index (0-7) or an
// `[r, g, b]` array.
function parseColor(value) {
  const basic = kBasicColors.get(value);
  if (basic !== undefined) {
    return basic;
  }
  const named = kNamedColors.get(value);
  if (named !== undefined) {
    return [(named >> 16) & 0xff, (named >> 8) & 0xff, named & 0xff];
  }
  if (StringPrototypeCharCodeAt(value, 0) === 35) { // '#'
    const match = RegExpPrototypeExec(kHexColorRegExp, value);
    return match === null ? null : parseHexColor(match[1]);
  }
  const match = RegExpPrototypeExec(kFunctionalColorRegExp, value);
  if (match === null) {
    return null;
  }
  const components = splitComponents(match[2]);
  if (components.length < 3 || components.length > 4) {
    return null;
  }
  if (match[1] === 'rgb' || match[1] === 'rgba') {
    const r = parseComponent(components[0], 255, false);
    const g = parseComponent(components[1], 255, false);
    const b = parseComponent(components[2], 255, false);
    if (NumberIsNaN(r) || NumberIsNaN(g) || NumberIsNaN(b)) {
      return null;
    }
    return [clampByte(r), clampByte(g), clampByte(b)];
  }
  const h = parseComponent(components[0], 360, true);
  const s = parseComponent(components[1], 1, false);
  const l = parseComponent(components[2], 1, false);
  if (NumberIsNaN(h) || NumberIsNaN(s) || NumberIsNaN(l)) {
    return null;
  }
  return hslToRgb(h, s, l);
}

function colorToSGR(color, base) {
  if (typeof color === 'number') {
    return `${base + color}`;
  }
  return `${base + 8};2;${color[0]};${color[1]};${color[2]}`;
}

// Splits a CSS declaration block into `[property, value]` pairs.
function parseDeclarations(css) {
  const declarations = [];
  let depth = 0;
  let start = 0;
  for (let i = 0; i <= css.length; i++) {
    const code = i === css.length ? 59 : StringPrototypeCharCodeAt(css, i);
    if (code === 40) { // '('
      depth++;
    } else if (code === 41) { // ')'
      if (depth > 0) depth--;
    } else if (code === 59 && depth === 0) { // ';'
      const declaration = StringPrototypeSlice(css, start, i);
      start = i + 1;
      const colon = StringPrototypeIndexOf(declaration, ':');
      if (colon === -1) {
        continue;
      }
      const property = StringPrototypeToLowerCase(
        StringPrototypeTrim(StringPrototypeSlice(declaration, 0, colon)));
      let value = StringPrototypeToLowerCase(
        StringPrototypeTrim(StringPrototypeSlice(declaration, colon + 1)));
      if (StringPrototypeEndsWith(value, '!important')) {
        value = StringPrototypeTrim(StringPrototypeSlice(value, 0, -10));
      }
      if (property !== '' && value !== '') {
        ArrayPrototypePush(declarations, [property, value]);
      }
    }
  }
  return declarations;
}

function isBoldWeight(value) {
  if (value === 'bold' || value === 'bolder') {
    return true;
  }
  if (value === 'normal' || value === 'lighter') {
    return false;
  }
  const weight = NumberParseFloat(value);
  return !NumberIsNaN(weight) && weight >= 600;
}

/**
 * Converts a CSS declaration block, as passed to the `%c` format specifier,
 * into the parameters of an ANSI SGR escape sequence.
 * @param {string} css
 * @returns {string}
 */
function cssToAnsi(css) {
  let color = null;
  let backgroundColor = null;
  let bold = false;
  let italic = false;
  let underline = false;
  let lineThrough = false;
  let overline = false;
  let decorationColor = null;

  const declarations = parseDeclarations(css);
  for (let i = 0; i < declarations.length; i++) {
    const { 0: property, 1: value } = declarations[i];
    switch (property) {
      case 'color':
        color = parseColor(value);
        break;
      case 'background-color':
      case 'background':
        backgroundColor = parseColor(value);
        break;
      case 'font-weight':
        bold = isBoldWeight(value);
        break;
      case 'font-style':
        italic = value === 'italic' || value === 'oblique';
        break;
      case 'text-decoration':
      case 'text-decoration-line': {
        underline = false;
        lineThrough = false;
        overline = false;
        if (property === 'text-decoration') {
          decorationColor = null;
        }
        const parts = StringPrototypeSplit(value, ' ');
        for (let j = 0; j < parts.length; j++) {
          const part = parts[j];
          if (part === 'underline') {
            underline = true;
          } else if (part === 'line-through') {
            lineThrough = true;
          } else if (part === 'overline') {
            overline = true;
          } else if (property === 'text-decoration' && part !== '') {
            const parsed = parseColor(part);
            if (parsed !== null) {
              decorationColor = parsed;
            }
          }
        }
        break;
      }
      case 'text-decoration-color':
        decorationColor = parseColor(value);
        break;
    }
  }

  let sgr = '';
  const append = (code) => {
    sgr += sgr === '' ? code : `;${code}`;
  };
  if (bold) append('1');
  if (italic) append('3');
  if (underline) append('4');
  if (lineThrough) append('9');
  if (overline) append('53');
  if (color !== null) append(colorToSGR(color, 30));
  if (backgroundColor !== null) append(colorToSGR(backgroundColor, 40));
  if (decorationColor !== null && (underline || lineThrough || overline)) {
    const rgb = typeof decorationColor === 'number' ?
      kBasicColorRGB[decorationColor] : decorationColor;
    append(`58;2;${rgb[0]};${rgb[1]};${rgb[2]}`);
  }
  return sgr;
}

module.exports = {
  cssToAnsi,
};
