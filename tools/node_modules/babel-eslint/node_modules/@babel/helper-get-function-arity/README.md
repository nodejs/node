# @babel/helper-get-function-arity

Function that returns the number of arguments that a function takes.
* Examples of what is considered an argument can be found at [MDN Web Docs](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Function/length)

## Usage

```javascript
import getFunctionArity from "@babel/helper-get-function-arity";

function wrap(state, method, id, scope) {
  // ...
  if (!t.isFunction(method)) {
    return false;
  }

  const argumentsLength = getFunctionArity(method);

  // ...
}
```
