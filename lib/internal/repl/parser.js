const acorn = require('internal/deps/acorn/dist/acorn');
const walk = require('internal/deps/acorn/dist/walk');

function parseFunctionArgs(input, r) {
  if(input === '') return [''];
  try {
      const parser = acorn.parse(input);
      const stroll = [];
      walk.simple(parser, {
        Literal(node) {
          stroll.push(node.value);
        }
      })
      if(typeof r !== 'undefined') stroll.push(r);
      return stroll;
  } catch(e) {
      const trimmedStr = input.trim();
      const lastArgIndex = input.lastIndexOf(',');
      const lastArg = input.substr(lastArgIndex + 1).trim();
      return parseFunctionArgs(input.substring(0, lastArgIndex), lastArg);
  }
}


module.exports = {
  parseFunctionArgs,
}
