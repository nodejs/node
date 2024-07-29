interface Bar {
  name: string;
  age: number;
}

class Foo<T> {
  constructor(private value: T) {}

  public getValue(): T {
    return this.value;
  }
}

const foo = new Foo<Bar>({ age: 42, name: 'John Doe' });
console.log(foo.getValue());
