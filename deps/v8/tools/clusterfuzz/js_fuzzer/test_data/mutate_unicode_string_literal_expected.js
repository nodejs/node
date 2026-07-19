// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original: mutate_unicode_string_literal.js
console.log("\u0041\u0020\x73\x69\u006d\u0070\x6c\u0065\u{20}\u0073\u{74}\u0072\u0069\u{6e}\x67\x20\u{6c}\u0069\u0074\x65\x72\u0061\u{6c}\x20\x66\u{6f}\u{72}\u{20}\u0074\u{65}\x73\u0074\x69\u{6e}\x67\x2e");
var text = '\x41\u006e\x6f\u{74}\x68\u0065\u0072\u0020\x6c\u{69}\x74\u0065\x72\u0061\u{6c}\u0020\u{77}\u{69}\x74\u{68}\x20\u0073\x69\u006e\u{67}\x6c\x65\u0020\u{71}\u0075\u006f\u0074\u0065\u{73}\u{20}\u{61}\u{6e}\u{64}\x20\u006e\u{75}\x6d\u0062\u0065\u0072\u{73}\x20\u0031\u0032\u{33}\u0034\x35\u{2e}';
function processString(input) {
  return "\u0070\x72\x6f\x63\x65\u{73}\u{73}\x65\x64\x3a\x20" + input;
}
processString("\x54\u0068\u0069\u{73}\x20\u{73}\x74\u0072\u{69}\u{6e}\x67\u{20}\u{69}\x73\u{20}\u0061\u{20}\u0066\u{75}\u006e\u0063\x74\u{69}\x6f\x6e\x20\u{61}\u0072\x67\u{75}\x6d\u0065\u{6e}\x74\x2e");
var obj = {
  "\x70\x72\u006f\u{70}\u{65}\u0072\u{74}\u0079\x2d\u006f\x6e\u0065": "\x76\u{61}\x6c\u0075\u{65}\x31",
  "\u0061\u002d\u{73}\u{65}\x63\x6f\x6e\u{64}\u{2d}\u{70}\x72\u006f\x70\u0065\x72\x74\x79": "\x76\u{61}\u{6c}\x75\x65\u0032"
};
var messages = ["\x66\x69\u0072\u{73}\u{74}\u{20}\u006d\x65\u0073\u{73}\u0061\u{67}\x65", "\x73\x65\u{63}\u{6f}\u006e\x64\x20\u{6d}\u0065\x73\u0073\u{61}\u0067\x65", "\x74\x68\u{69}\u{72}\u{64}\u0020\x6d\u0065\x73\u{73}\u0061\u{67}\u0065"];
var longString = "\u{54}\x68\u0069\u0073\x20\u0069\x73\x20\x61\u{20}\u006d\u{75}\x63\u0068\u0020\u006c\x6f\u006e\u{67}\u{65}\x72\u0020\u0073\u0074\x72\u0069\u006e\u{67}\x20\u0074\x6f\u0020\x70\u{72}\u{6f}\u0076\u{69}\u0064\u{65}\x20\u{6d}\u{6f}\u{72}\u0065\x20\u{6f}\u0070\x70\u{6f}\u0072\u{74}\x75\u{6e}\u0069\u0074\u0069\u{65}\u{73}\x20\u0066\u{6f}\x72\u{20}\x63\x68\u{61}\u0072\u{61}\u{63}\u0074\u{65}\u{72}\x20\u006d\u0075\u{74}\u0061\u{74}\u{69}\u{6f}\u{6e}\x73\x20\u0074\x6f\u{20}\x6f\x63\u{63}\u0075\x72\u{20}\u{62}\u{61}\x73\u{65}\u{64}\u{20}\u006f\u{6e}\x20\x74\u{68}\u0065\u{20}\x70\x72\x6f\u0062\u{61}\x62\x69\u006c\u0069\x74\x79\u{20}\x73\u0065\x74\u0074\u0069\u006e\x67\x73\u002e";
var emojiString = "\x54\u0065\u0073\u0074\u0069\u006e\x67\u{20}\u{77}\x69\u{74}\x68\u{20}\u0061\x6e\u{20}\x65\x6d\u{6f}\u006a\u{69}\x20\u{63}\x68\u{61}\x72\x61\x63\u{74}\x65\x72\x20\u{1f642}\u{20}\u{74}\x6f\u0020\x65\u{6e}\x73\u0075\x72\u{65}\u{20}\u0063\u006f\x72\u{72}\u{65}\u{63}\x74\x20\u0068\u{61}\x6e\u{64}\u006c\u0069\x6e\u0067\u002e";
eval('\u0076\x61\x72\u0020\u{64}\u0079\x6e\x61\x6d\u{69}\u{63}\u{56}\x61\u{72}\u0020\u003d\u{20}\u0022\x61\x20\x73\u{74}\x72\u0069\x6e\u{67}\u{20}\x69\u{6e}\u{73}\u0069\u{64}\u0065\u{20}\x65\x76\x61\x6c\x22\u{3b}');
const simpleTemplate = `\x54\u0068\u0069\x73\x20\x69\x73\u{20}\u{61}\u{20}\x73\u{69}\u{6d}\u0070\x6c\u{65}\u{20}\u{74}\x65\u{6d}\u{70}\x6c\u{61}\x74\x65\u0020\u006c\x69\x74\u0065\u{72}\x61\u006c\u002e`;
console.log(simpleTemplate);
const user = "\u0041\u006c\u{69}\u{63}\u0065";
const templateWithExpression = `\u0055\u0073\u0065\u0072\x20\x70\x72\u{6f}\u{66}\u0069\x6c\u{65}\u0020\u0066\x6f\x72\u0020${user}\u0020\x69\x73\u0020\u{6e}\x6f\u{77}\x20\u{61}\u{63}\u0074\u0069\u0076\u0065\u002e`;
console.log(templateWithExpression);
const multiLineTemplate = `\x54\u0068\u{69}\x73\u{20}\x69\x73\u0020\x74\u{68}\u0065\x20\x66\x69\u{72}\x73\x74\u0020\u006c\u0069\u{6e}\u{65}\u0020\u{6f}\u0066\x20\u{74}\u0065\u0078\u{74}\u{2e}\x0a\u{54}\u0068\u{69}\u0073\u0020\x69\x73\u{20}\x74\x68\x65\u{20}\u{73}\u{65}\x63\u{6f}\u{6e}\x64\u0020\u006c\u0069\u{6e}\u0065\u0020\u006f\x66\x20\u{74}\u0065\u0078\x74\u{2e}\u000a\u0041\x6e\u0064\u0020\u{74}\x68\x69\u0073\u0020\u0069\u{73}\x20\u0074\u0068\u0065\u{20}\u{66}\u{69}\u006e\x61\u006c\u{20}\x6c\u{69}\u006e\x65\u002c\u{20}\u{6e}\u{75}\u{6d}\x62\u0065\x72\x20\x33\x2e`;
console.log(multiLineTemplate);
function customTag(strings, ...values) {
  let str = "";
  strings.forEach((string, i) => {
    str += string + (values[i] || "");
  });
  return str;
}
const taggedTemplate = customTag`\u0049\u0074\u0065\u006d\u0020\x49\u{44}\u{3a}\u{20}${12345}\x2c\u{20}\u0053\u{74}\u{61}\u{74}\u0075\x73\u{3a}\x20\u{4f}\x4b\u002e`;
console.log(taggedTemplate);
