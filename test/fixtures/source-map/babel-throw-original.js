/*---
esid: prod-OptionalExpression
features: [optional-chaining]
---*/

const obj = {
  a: {
    b: 22
  }
};

function fn () {
  return {};
}

setTimeout((err) => {
  // OptionalExpression (MemberExpression OptionalChain) OptionalChain
  if (obj?.a?.b === 22) throw Error('an exception');
}, 5);
