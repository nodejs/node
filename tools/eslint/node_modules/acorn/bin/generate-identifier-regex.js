// Which Unicode version should be used?
var version = '8.0.0';

var start = require('unicode-' + version + '/properties/ID_Start/code-points')
    .filter(function(ch) { return ch > 127; });
var last = -1;
var cont = [0x200c, 0x200d].concat(require('unicode-' + version + '/properties/ID_Continue/code-points')
    .filter(function(ch) { return ch > 127 && search(start, ch, last + 1) == -1; }));

function search(arr, ch, starting) {
  for (var i = starting; arr[i] <= ch && i < arr.length; last = i++)
    if (arr[i] === ch)
      return i;
  return -1;
}

function pad(str, width) {
  while (str.length < width) str = "0" + str;
  return str;
}

function esc(code) {
  var hex = code.toString(16);
  if (hex.length <= 2) return "\\x" + pad(hex, 2);
  else return "\\u" + pad(hex, 4);
}

function generate(chars) {
  var astral = [], re = "";
  for (var i = 0, at = 0x10000; i < chars.length; i++) {
    var from = chars[i], to = from;
    while (i < chars.length - 1 && chars[i + 1] == to + 1) {
      i++;
      to++;
    }
    if (to <= 0xffff) {
      if (from == to) re += esc(from);
      else if (from + 1 == to) re += esc(from) + esc(to);
      else re += esc(from) + "-" + esc(to);
    } else {
      astral.push(from - at, to - from);
      at = to;
    }
  }
  return {nonASCII: re, astral: astral};
}

var startData = generate(start), contData = generate(cont);

console.log("let nonASCIIidentifierStartChars = \"" + startData.nonASCII + "\"");
console.log("let nonASCIIidentifierChars = \"" + contData.nonASCII + "\"");
console.log("const astralIdentifierStartCodes = " + JSON.stringify(startData.astral));
console.log("const astralIdentifierCodes = " + JSON.stringify(contData.astral));
