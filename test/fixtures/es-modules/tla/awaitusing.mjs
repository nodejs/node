export const ready = true;

await using lock = {
  async [Symbol.asyncDispose]() {},
};
