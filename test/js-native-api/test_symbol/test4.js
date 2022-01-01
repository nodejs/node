'use strict';
const common = require('../../common');
const {
  deepStrictEqual,
  strictEqual,
} = require('assert');

// Testing api calls for test_symbol.ToStringTag
const { ToStringTag } = require(`./build/${common.buildType}/test_symbol`);

const toStringTagSymbol = ToStringTag();
strictEqual(typeof toStringTagSymbol, 'symbol');
strictEqual(toStringTagSymbol, ToStringTag());
strictEqual(toStringTagSymbol, Symbol.toStringTag);
strictEqual(toStringTagSymbol.toString(), 'Symbol(Symbol.toStringTag)');

{
  const tag = 'MyTag';
  const object = { [toStringTagSymbol]: tag };

  deepStrictEqual(Object.keys(object), []);
  deepStrictEqual(Object.getOwnPropertyNames(object), []);
  deepStrictEqual(Object.getOwnPropertySymbols(object), [ Symbol.toStringTag ]);

  strictEqual(object[toStringTagSymbol], tag);
  strictEqual(object.toString(), `[object ${tag}]`);
}

{
  const tag = 5;
  const object = { [toStringTagSymbol]: tag };

  deepStrictEqual(Object.keys(object), []);
  deepStrictEqual(Object.getOwnPropertyNames(object), []);
  deepStrictEqual(Object.getOwnPropertySymbols(object), [ Symbol.toStringTag ]);

  strictEqual(object[toStringTagSymbol], tag);
  strictEqual(object.toString(), '[object Object]');
}
