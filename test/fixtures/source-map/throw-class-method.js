class Foo {
  bar() {
    throw Error('This is a test');
  }
}

class Bar {}
Bar.prototype.bar = Foo.prototype.bar;

try {
  const foo = new Foo();
  foo.bar();
} catch (e) {
  console.error(e);
}

try {
  const bar = Object.create(Bar.prototype);
  bar.bar();
} catch (e) {
  console.error(e);
}

// To recreate:
//
// cd test/fixtures/source-map
// npx terser -o throw-class-method.min.js --source-map "url='throw-class-method.min.js.map'" throw-class-method.js
