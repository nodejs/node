esmangle-evaluator
==================

[`esmangle`](https://github.com/estools/esmangle)'s `evaluator.js` as a standalone module.

`evaluator.js` is an unmodified copy of the [original](https://github.com/Constellation/esmangle/blob/7e4ee597/lib/evaluator.js).

`common.js` is a stub of the [original](https://github.com/Constellation/esmangle/blob/7e4ee597/lib/common.js) with a few helpers/constants required by `evaluator.js`.

## Exports

```js
{
  constant: {
    doBinary: [Function: doBinary],
    doUnary: [Function: doUnary],
    doLogical: [Function: doLogical],
    evaluate: [Function: getConstant],
    isConstant: [Function: isConstant]
  },
  hasSideEffect: [Function: hasSideEffect],
  booleanCondition: [Function: booleanCondition]
}
```
