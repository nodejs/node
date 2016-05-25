const common = require('../common.js');
const parseAPIHeader = require('./parseAPIHeader.js');
const parseYAML = require('./parseYAML.js');
// just update the list item text in-place.
// lists that come right after a heading are what we're after.
module.exports = function parseLists(input) {
  var state = null;
  var depth = 0;
  var output = [];
  output.links = input.links;
  input.forEach(function(tok) {
    if (tok.type === 'code' && tok.text.match(/Stability:.*/g)) {
      tok.text = parseAPIHeader(tok.text);
      output.push({ type: 'html', text: tok.text });
      return;
    }
    if (state === null ||
      (state === 'AFTERHEADING' && tok.type === 'heading')) {
      if (tok.type === 'heading') {
        state = 'AFTERHEADING';
      }
      output.push(tok);
      return;
    }
    if (state === 'AFTERHEADING') {
      if (tok.type === 'list_start') {
        state = 'LIST';
        if (depth === 0) {
          output.push({ type: 'html', text: '<div class="signature">' });
        }
        depth++;
        output.push(tok);
        return;
      }
      if (tok.type === 'html' && common.isYAMLBlock(tok.text)) {
        tok.text = parseYAML(tok.text);
      }
      state = null;
      output.push(tok);
      return;
    }
    if (state === 'LIST') {
      if (tok.type === 'list_start') {
        depth++;
        output.push(tok);
        return;
      }
      if (tok.type === 'list_end') {
        depth--;
        output.push(tok);
        if (depth === 0) {
          state = null;
          output.push({ type: 'html', text: '</div>' });
        }
        return;
      }
    }
    output.push(tok);
  });

  return output;
}
