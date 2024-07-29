type MyType = {
  foo: string;
  bar: number;
  zoo: boolean;
  metadata?: unknown;
};

type Role = 'admin' | 'user' | 'manager';

type RoleAttributes =
  | {
      role: 'admin';
      permission: 'all';
    }
  | {
      role: 'user';
    }
  | {
      role: 'manager';
    };

type MaybeString = string | null | undefined;

type myFunction = (a: number, b: number) => number;

type PromiseMyType = Promise<MyType>;

// Union Type: Partial
type PartialType = Partial<MyType>;

const PartialTypeWithValues: PartialType = {
  foo: 'Hello, TypeScript!',
  bar: 42,
  zoo: true,
  metadata: undefined,
};

const PartialTypesWithoutRequiredValues: PartialType = {
  foo: 'Hello, TypeScript!',
};

console.log(PartialTypeWithValues);
console.log(PartialTypesWithoutRequiredValues);

// Union Type: Required
type RequiredType = Required<MyType>;

const RequiredTypeWithValues: MyType = {
  foo: 'Hello, TypeScript!',
  bar: 42,
  zoo: true,
  metadata: undefined,
};

const RequiredTypes: RequiredType = {
  foo: 'Hello, TypeScript!',
  bar: 42,
  zoo: true,
  metadata: {
    name: 'some value',
  },
};

console.log(RequiredTypeWithValues);
console.log(RequiredTypes);

// Union Type: Readonly
type ReadonlyType = Readonly<MyType>;

const ReadonlyTypeWithValues: ReadonlyType = {
  foo: 'Hello, TypeScript!',
  bar: 42,
  zoo: true,
  metadata: undefined,
};

// It throw an error because it is a readonly type

//readonlyTypeWithValues.foo = 'Hello, TypeScript!';

// Union Type: Pick
type PickType = Pick<MyType, 'foo' | 'bar'>;

const PickTypeWithValues: PickType = {
  foo: 'Hello, TypeScript!',
  bar: 42,
};

console.log(PickTypeWithValues);

// Union Type: Omit
type OmitType = Omit<MyType, 'foo' | 'metadata'>;

const OmitTypeWithValues: OmitType = {
  bar: 42,
  zoo: true,
};

console.log(OmitTypeWithValues);

// Union Type: Exclude

type NoUserRole = Exclude<Role, 'user'>;

// user not exists in the NoUserRole
const noUserRole: NoUserRole = 'admin';
console.log(noUserRole);

// Union Type: Extract
type AdminRole = Extract<RoleAttributes, { role: 'admin' }>;
const adminRole: AdminRole = {
  role: 'admin',
  permission: 'all',
};
console.log(adminRole);

// Union Type: NonNullable
type NonNullableString = NonNullable<MaybeString>;

const nonNullableString: NonNullableString = 'Hello, TypeScript!';
console.log(nonNullableString);

// Union Type: Return Type
type ReturnValueFunction = ReturnType<myFunction>;

// it will return number
const returnValueFunction: ReturnValueFunction = 42;
console.log(returnValueFunction);

// Union Type: Parameters
type FunctionParameters = Parameters<myFunction>;
const functionParameters: FunctionParameters = [1, 2];
console.log(functionParameters);

// Union Type: Awaited

type ResultPromiseMyType = Awaited<PromiseMyType>;
const resultPromiseMyType: ResultPromiseMyType = {
  bar: 42,
  zoo: true,
  foo: 'Hello, TypeScript!',
  metadata: undefined,
};

console.log(resultPromiseMyType);

// Use Some Union Types Together
const getTypescript = async () => {
  return {
    name: 'Hello, TypeScript!',
  };
};

type MyNameResult = Awaited<ReturnType<typeof getTypescript>>;
const myNameResult: MyNameResult = {
  name: 'Hello, TypeScript!',
};

console.log(myNameResult);
