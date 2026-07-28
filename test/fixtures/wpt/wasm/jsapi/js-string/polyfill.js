// Generate some helper functions for manipulating (array (mut i16)) from JS
let helperExports;
{
  const builder = new WasmModuleBuilder();
  const arrayIndex = builder.addArray(kWasmI16, true, kNoSuperType, true);

  builder
    .addFunction("createArrayMutI16", {
      params: [kWasmI32],
      results: [kWasmAnyRef]
    })
    .addBody([
      kExprLocalGet,
      ...wasmSignedLeb(0),
      ...GCInstr(kExprArrayNewDefault),
      ...wasmSignedLeb(arrayIndex)
      ])
    .exportFunc();

  builder
    .addFunction("arrayLength", {
      params: [kWasmArrayRef],
      results: [kWasmI32]
    })
    .addBody([
      kExprLocalGet,
      ...wasmSignedLeb(0),
      ...GCInstr(kExprArrayLen)
      ])
    .exportFunc();

  builder
    .addFunction("arraySet", {
      params: [wasmRefNullType(arrayIndex), kWasmI32, kWasmI32],
      results: []
    })
    .addBody([
      kExprLocalGet,
      ...wasmSignedLeb(0),
      kExprLocalGet,
      ...wasmSignedLeb(1),
      kExprLocalGet,
      ...wasmSignedLeb(2),
      ...GCInstr(kExprArraySet),
      ...wasmSignedLeb(arrayIndex)
      ])
    .exportFunc();

  builder
    .addFunction("arrayGet", {
      params: [wasmRefNullType(arrayIndex), kWasmI32],
      results: [kWasmI32]
    })
    .addBody([
      kExprLocalGet,
      ...wasmSignedLeb(0),
      kExprLocalGet,
      ...wasmSignedLeb(1),
      ...GCInstr(kExprArrayGetU),
      ...wasmSignedLeb(arrayIndex)
      ])
    .exportFunc();

  let bytes = builder.toBuffer();
  let module = new WebAssembly.Module(bytes);
  let instance = new WebAssembly.Instance(module);

  helperExports = instance.exports;
}

function throwIfNotString(a) {
  if (typeof a !== "string") {
    throw new WebAssembly.RuntimeError();
  }
}

this.polyfillImports = {
  test: (string) => {
    if (string === null ||
        typeof string !== "string") {
      return 0;
    }
    return 1;
  },
  cast: (string) => {
    throwIfNotString(string);
    return string;
  },
  fromCharCodeArray: (array, arrayStart, arrayCount) => {
    arrayStart >>>= 0;
    arrayCount >>>= 0;
    let length = helperExports.arrayLength(array);
    if (BigInt(arrayStart) + BigInt(arrayCount) > BigInt(length)) {
      throw new WebAssembly.RuntimeError();
    }
    let result = '';
    for (let i = arrayStart; i < arrayStart + arrayCount; i++) {
      result += String.fromCharCode(helperExports.arrayGet(array, i));
    }
    return result;
  },
  intoCharCodeArray: (string, arr, arrayStart) => {
    arrayStart >>>= 0;
    throwIfNotString(string);
    let arrLength = helperExports.arrayLength(arr);
    let stringLength = string.length;
    if (BigInt(arrayStart) + BigInt(stringLength) > BigInt(arrLength)) {
      throw new WebAssembly.RuntimeError();
    }
    for (let i = 0; i < stringLength; i++) {
      helperExports.arraySet(arr, arrayStart + i, string[i].charCodeAt(0));
    }
    return stringLength;
  },
  fromCharCode: (charCode) => {
    charCode >>>= 0;
    return String.fromCharCode(charCode);
  },
  fromCodePoint: (codePoint) => {
    codePoint >>>= 0;
    return String.fromCodePoint(codePoint);
  },
  charCodeAt: (string, stringIndex) => {
    stringIndex >>>= 0;
    throwIfNotString(string);
    if (stringIndex >= string.length)
      throw new WebAssembly.RuntimeError();
    return string.charCodeAt(stringIndex);
  },
  codePointAt: (string, stringIndex) => {
    stringIndex >>>= 0;
    throwIfNotString(string);
    if (stringIndex >= string.length)
      throw new WebAssembly.RuntimeError();
    return string.codePointAt(stringIndex);
  },
  length: (string) => {
    throwIfNotString(string);
    return string.length;
  },
  concat: (stringA, stringB) => {
    throwIfNotString(stringA);
    throwIfNotString(stringB);
    return stringA + stringB;
  },
  substring: (string, startIndex, endIndex) => {
    startIndex >>>= 0;
    endIndex >>>= 0;
    throwIfNotString(string);
    if (startIndex > string.length,
        endIndex > string.length,
        endIndex < startIndex) {
      return "";
    }
    return string.substring(startIndex, endIndex);
  },
  equals: (stringA, stringB) => {
    if (stringA !== null) throwIfNotString(stringA);
    if (stringB !== null) throwIfNotString(stringB);
    return stringA === stringB;
  },
  compare: (stringA, stringB) => {
    throwIfNotString(stringA);
    throwIfNotString(stringB);
    if (stringA < stringB) {
      return -1;
    }
    return stringA === stringB ? 0 : 1;
  },
};
