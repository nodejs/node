## JSON5 syntax

We support slighly modified version of JSON5, see https://groups.google.com/forum/#!topic/json5/3DjClVYI6Wg

I started from ES5 specification and added a set of additional restrictions on top of ES5 spec. So I'd expect my implementation to be much closer to javascript. It's no longer an extension of json, but a reduction of ecmascript, which was my original intent.

This JSON5 version is a subset of ES5 language, specification is here:

http://www.ecma-international.org/publications/files/ECMA-ST/Ecma-262.pdf

This is a language that defines data structures only, so following notes/restrictions are applied:

- Literals (NullLiteral, BooleanLiteral, NumericLiteral, StringLiteral) are allowed.
- Compatibility syntax is not supported, which means octal literals are forbidden.
- ArrayLiterals and allowed, but instead of AssignmentExpressions you can only use other allowed Literals, ArrayLiterals and ObjectLiterals. Elisions are currently not supported.
- ObjectLiterals and allowed, but instead of AssignmentExpressions you can only use other allowed Literals, ArrayLiterals and ObjectLiterals. Setters and getters are forbidden.
- All other primary expressions ("this", Identifier, Expression) are forbidden.
- Two unary expressions ('-' and '+') allowed before NumericLiterals.
- Any data that has a number type can be represented, including +0, -0, +Infinity, -Infinity and NaN.
- "undefined" is forbidden, use null instead if applicable.
- Comments and whitespace are defined according to spec.

Main authority here is ES5 spec, so strict backward JSON compatibility is not guaranteed.


If you're unsure whether a behaviour of this library is a bug or not, you can run this test:

```javascript
JSON5.parse(String(something))
```

Should always be equal to:

```javascript
eval('(function(){return ('+String(something)+'\n)\n})()')
```

If `something` meets all rules above. Parens and newlines in the example above are carefully placed so comments and another newlines will work properly, so don't look so impressed about that.


## Weirdness of JSON5

These are the parts that I don't particulary like, but see no good way to fix:

 - no elisions, `[,,,] -> [null,null,null]`
 - `[Object], [Circular]` aren't parsed
 - no way of nicely representing multiline strings
 - unicode property names are way to hard to implement
 - Date and other custom objects
 - incompatible with YAML (at least comments)
