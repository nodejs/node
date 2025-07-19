class Foo {
  bar() {
    console.trace('This is a test');
  }
}

class Bar {}
Bar.prototype.bar = Foo.prototype.bar;

const foo = new Foo();
foo.bar();
const bar = Object.create(Bar.prototype);
bar.bar();

// To recreate:
//
// cd test/fixtures/source-map
// npx terser -o throw-class-method.min.js --source-map "url='throw-class-method.min.js.map'" throw-class-method.js
