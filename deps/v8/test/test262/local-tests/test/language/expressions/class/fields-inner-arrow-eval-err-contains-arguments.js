/*---
description: error if `arguments` in StatementList of eval (direct eval)
esid: sec-performeval-rules-in-initializer
features: [class, class-fields-public, arrow-function]
flags: [generated]
info: |
    Static Semantics: Early Errors

      FieldDefinition:
        PropertyNameInitializeropt

      - It is a Syntax Error if ContainsArguments of Initializer is true.

    Static Semantics: ContainsArguments
      IdentifierReference : Identifier

      1. If the StringValue of Identifier is "arguments", return true.
      ...
      For all other grammatical productions, recurse on all nonterminals. If any piece returns true, then return true. Otherwise return false.

---*/

var C = class {
  x = () => {
    var t = () => { eval("arguments"); };
    t();
  }
}

assert.throws(SyntaxError, function() {
  var c = new C();
  c.x();
});
