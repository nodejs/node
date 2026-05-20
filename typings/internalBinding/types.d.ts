export interface TypesBinding {
  isArgumentsObject(value: unknown): value is IArguments;
  isArrayBuffer(value: unknown): value is ArrayBuffer;
  isAsyncFunction(value: unknown): value is (...args: unknown[]) => Promise<unknown>;
  isBigIntObject: (value: unknown) => value is BigInt;
  isBooleanObject: (value: unknown) => value is Boolean;
  isDataView(value: unknown): value is DataView;
  isDate: (value: unknown) => value is Date;
  isExternal(value: unknown): value is object;
  isGeneratorFunction(value: unknown): value is GeneratorFunction;
  isGeneratorObject(value: unknown): value is Generator;
  isMap(value: unknown): value is Map<unknown, unknown>;
  isMapIterator: (value: unknown) => value is IterableIterator<unknown>;
  isModuleNamespaceObject: (value: unknown) => value is { [Symbol.toStringTag]: 'Module' };
  isNativeError: (value: unknown) => value is Error;
  isNumberObject: (value: unknown) => value is Number;
  isPromise: (value: unknown) => value is Promise<unknown>;
  isProxy: (value: unknown) => value is object;
  isRegExp: (value: unknown) => value is RegExp;
  isSet: (value: unknown) => value is Set<unknown>;
  isSetIterator: (value: unknown) => value is IterableIterator<unknown>;
  isSharedArrayBuffer: (value: unknown) => value is SharedArrayBuffer;
  isStringObject: (value: unknown) => value is String;
  isSymbolObject: (value: unknown) => value is Symbol;
  isWeakMap: (value: unknown) => value is WeakMap<object, unknown>;
  isWeakSet: (value: unknown) => value is WeakSet<object>;
  isAnyArrayBuffer(value: unknown): value is ArrayBufferLike;
  isBoxedPrimitive(value: unknown): value is BigInt | Boolean | Number | String | Symbol;
}
