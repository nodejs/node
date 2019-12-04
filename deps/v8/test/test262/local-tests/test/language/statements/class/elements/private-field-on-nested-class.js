// This file was procedurally generated from the following sources:
// - src/class-elements/private-field-on-nested-class.case
// - src/class-elements/default/cls-decl.template
/*---
description: PrivateName CallExpression usage (private field) (field definitions in a class declaration)
esid: prod-FieldDefinition
features: [class-fields-private, class-fields-public, class]
flags: [generated]
info: |
    Updated Productions

    CallExpression[Yield, Await]:
      CoverCallExpressionAndAsyncArrowHead[?Yield, ?Await]
      SuperCall[?Yield, ?Await]
      CallExpression[?Yield, ?Await]Arguments[?Yield, ?Await]
      CallExpression[?Yield, ?Await][Expression[+In, ?Yield, ?Await]]
      CallExpression[?Yield, ?Await].IdentifierName
      CallExpression[?Yield, ?Await]TemplateLiteral[?Yield, ?Await]
      CallExpression[?Yield, ?Await].PrivateName

---*/


class C {
  #outer = 'test262';

  B_withoutPrivateField = class {
    method(o) {
      return o.#outer;
    }
  }

  B_withPrivateField = class {
    #inner = 42;
    method(o) {
      return o.#outer;
    }
  }
}

let c = new C();
let innerB1 = new c.B_withoutPrivateField();
assert.sameValue(innerB1.method(c), 'test262');
let innerB2 = new c.B_withPrivateField();
assert.sameValue(innerB2.method(c), 'test262');
