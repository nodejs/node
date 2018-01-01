const acorn = require('internal/deps/acorn/dist/acorn');
const walk = require('internal/deps/acorn/dist/walk');
const acornLoose = require('internal/deps/acorn/dist/acorn_loose');

// function parseFunctionArgs(argsString) {
//   var closedArg = /^(?:[^"\\]|\\.|"(?:\\.|[^"\\])*")*$/;

//   let args = argsString.split(',');
//   let results = [];

//   while(args.length) {
//     const curArg = args.shift();
//     if(closedArg.test(curArg) || !args.length) {
//       results.push(curArg);
//     } else {
//       args[0] = curArg + ',' + args[0];
//     }
//   }

//   return results;
// }

// function parseFunctionArgs(str, r) {
//   if(str === '') return [''];
//   const node = acornLoose.parse_dammit(str).body[0].expression;
//   if(node.expressions) {
//     return node.expressions.map(node => node.value)
//   }
//   return [node.value];
// }

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
