export interface TypesBinding {
  isAsyncFunction(value: unknown): value is (...args: unknown[]) => Promise<unknown>;
  isGeneratorFunction(value: unknown): value is GeneratorFunction;
  isAnyArrayBuffer(value: unknown): value is (ArrayBuffer | SharedArrayBuffer);
  isArrayBuffer(value: unknown): value is ArrayBuffer;
  isArgumentsObject(value: unknown): value is ArrayLike<unknown>;
  isBoxedPrimitive(value: unknown): value is (BigInt | Boolean | Number | String | Symbol);
  isDataView(value: unknown): value is DataView;
  isExternal(value: unknown): value is Object;
  isMap(value: unknown): value is Map<unknown, unknown>;
  isMapIterator: (value: unknown) => value is IterableIterator<unknown>;
  isModuleNamespaceObject: (value: unknown) => value is { [Symbol.toStringTag]: 'Module' };
  isNativeError: (value: unknown) => Error;
  isPromise: (value: unknown) => value is Promise<unknown>;
  isSet: (value: unknown) => value is Set<unknown>;
  isSetIterator: (value: unknown) => value is IterableIterator<unknown>;
  isWeakMap: (value: unknown) => value is WeakMap<object, unknown>;
  isWeakSet: (value: unknown) => value is WeakSet<object>;
  isRegExp: (value: unknown) => RegExp;
  isDate: (value: unknown) => Date;
  isTypedArray: (value: unknown) => value is TypedArray;
  isStringObject: (value: unknown) => value is String;
  isNumberObject: (value: unknown) => value is Number;
  isBooleanObject: (value: unknown) => value is Boolean,
  isBigIntObject: (value: unknown) => value is BigInt;
}
