const i32_value = 42;
export { i32_value as "🚀i32_value" };
export const i64_value = 9223372036854775807n;
export const f32_value = 3.14159;
export const f64_value = 3.141592653589793;

export const i32_mut_value = new WebAssembly.Global(
  { value: "i32", mutable: true },
  100
);
export const i64_mut_value = new WebAssembly.Global(
  { value: "i64", mutable: true },
  200n
);
export const f32_mut_value = new WebAssembly.Global(
  { value: "f32", mutable: true },
  2.71828
);
export const f64_mut_value = new WebAssembly.Global(
  { value: "f64", mutable: true },
  2.718281828459045
);

export const externref_value = { hello: "world" };
export const externref_mut_value = new WebAssembly.Global(
  { value: "externref", mutable: true },
  { mutable: "global" }
);
export const null_externref_value = null;
