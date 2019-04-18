// In JavaScript, reserved words cannot be identifiers (the `foo` in `var foo`)
// but they can be properties (`obj.foo`). This file checks that the `import`
// reserved word isn't incorrectly detected as a keyword. For more info see:
// https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Lexical_grammar#Reserved_word_usage

global.import = 3;

global['import'] = 3;

const obj = {
import: 3 // Specifically at column 0, to try to trick the detector
}

console.log(require('process').version);
