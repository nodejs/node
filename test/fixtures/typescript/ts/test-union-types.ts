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
