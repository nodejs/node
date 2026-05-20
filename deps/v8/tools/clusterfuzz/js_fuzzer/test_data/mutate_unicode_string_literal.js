// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

console.log("A simple string literal for testing.");

var text = 'Another literal with single quotes and numbers 12345.';

function processString(input) {
  return "processed: " + input;
}

processString("This string is a function argument.");

var obj = {
  "property-one": "value1",
  "a-second-property": "value2"
};

var messages = ["first message", "second message", "third message"];

var longString = "This is a much longer string to provide more opportunities for character mutations to occur based on the probability settings.";

var emojiString = "Testing with an emoji character 🙂 to ensure correct handling.";

eval('var dynamicVar = "a string inside eval";');

const simpleTemplate = `This is a simple template literal.`;
console.log(simpleTemplate);

const user = "Alice";
const templateWithExpression = `User profile for ${user} is now active.`;
console.log(templateWithExpression);

const multiLineTemplate = `This is the first line of text.
This is the second line of text.
And this is the final line, number 3.`;
console.log(multiLineTemplate);

function customTag(strings, ...values) {
  let str = "";
  strings.forEach((string, i) => {
    str += string + (values[i] || "");
  });
  return str;
}
const taggedTemplate = customTag`Item ID: ${12345}, Status: OK.`;
console.log(taggedTemplate);
