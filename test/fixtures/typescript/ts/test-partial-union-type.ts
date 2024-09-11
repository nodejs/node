type MyType = {
  foo: string;
  bar: number;
  zoo: boolean;
  metadata?: unknown;
};

// Union Type: Partial
type PartialType = Partial<MyType>;

const PartialTypeWithValues: PartialType = {
  foo: 'Hello, TypeScript!',
  bar: 42,
  zoo: true,
  metadata: undefined,
};

console.log(PartialTypeWithValues);
