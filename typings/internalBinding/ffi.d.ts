declare namespace InternalFFIBinding {
  type Buffer = Uint8Array;
  type BufferSource = Buffer | ArrayBuffer | ArrayBufferView | SharedArrayBuffer;
  type VoidType = 'void';
  type NumberType =
    | 'char'
    | 'int8' | 'i8'
    | 'uint8' | 'u8' | 'bool'
    | 'int16' | 'i16'
    | 'uint16' | 'u16'
    | 'int32' | 'i32'
    | 'uint32' | 'u32'
    | 'float32' | 'f32' | 'float'
    | 'float64' | 'f64' | 'double';
  type BigIntType = 'int64' | 'i64' | 'uint64' | 'u64';
  type PointerType =
    | 'pointer' | 'ptr'
    | 'string' | 'str'
    | 'buffer' | 'arraybuffer'
    | 'function';
  type ValueType = NumberType | BigIntType | PointerType;
  type ReturnType = VoidType | ValueType;
  type FFICallback = (...args: any[]) => unknown;

  interface FFIFunction {
    (...args: unknown[]): unknown;
    pointer: bigint;
  }
  interface FunctionSignature {
    arguments?: readonly ValueType[];
    return?: ReturnType;
  }

  class DynamicLibrary {
    constructor(path: string | null);

    readonly path: string;
    readonly symbols: Record<string, bigint>;
    readonly functions: Record<string, FFIFunction>;

    close(): void;
    getFunction(name: string, signature: FunctionSignature): FFIFunction;
    getFunctions(): Record<string, FFIFunction>;
    getFunctions(
      definitions: Record<string, FunctionSignature>,
    ): Record<string, FFIFunction>;
    getSymbol(name: string): bigint;
    getSymbols(): Record<string, bigint>;
    registerCallback(callback: FFICallback): bigint;
    registerCallback(
      signature: FunctionSignature,
      callback: FFICallback
    ): bigint;
    unregisterCallback(pointer: bigint): void;
    refCallback(pointer: bigint): void;
    unrefCallback(pointer: bigint): void;
    [Symbol.dispose](): void;
  }
}
export interface FFIBinding {
  DynamicLibrary: typeof InternalFFIBinding.DynamicLibrary;
  toString(pointer: bigint): string | null;
  toBuffer(pointer: bigint, length: number, copy?: boolean): InternalFFIBinding.Buffer;
  toArrayBuffer(pointer: bigint, length: number, copy?: boolean): ArrayBuffer;

  exportBytes(source: InternalFFIBinding.BufferSource, pointer: bigint, length: number): void;
  getRawPointer(source: InternalFFIBinding.BufferSource): bigint;
  getCurrentEventLoop(): bigint;

  getInt8(pointer: bigint, offset?: number): number;
  getUint8(pointer: bigint, offset?: number): number;
  getInt16(pointer: bigint, offset?: number): number;
  getUint16(pointer: bigint, offset?: number): number;
  getInt32(pointer: bigint, offset?: number): number;
  getUint32(pointer: bigint, offset?: number): number;
  getInt64(pointer: bigint, offset?: number): bigint;
  getUint64(pointer: bigint, offset?: number): bigint;
  getFloat32(pointer: bigint, offset?: number): number;
  getFloat64(pointer: bigint, offset?: number): number;

  setInt8(pointer: bigint, offset: number, value: number): void;
  setUint8(pointer: bigint, offset: number, value: number): void;
  setInt16(pointer: bigint, offset: number, value: number): void;
  setUint16(pointer: bigint, offset: number, value: number): void;
  setInt32(pointer: bigint, offset: number, value: number): void;
  setUint32(pointer: bigint, offset: number, value: number): void;
  setInt64(pointer: bigint, offset: number, value: number | bigint): void;
  setUint64(pointer: bigint, offset: number, value: number | bigint): void;
  setFloat32(pointer: bigint, offset: number, value: number): void;
  setFloat64(pointer: bigint, offset: number, value: number): void;

  charIsSigned: boolean;
  uintptrMax: bigint;

  kSbSharedBuffer: symbol;
  kSbInvokeSlow: symbol;
  kSbArguments: symbol;
  kSbReturn: symbol;
  kFastArguments: symbol;
  kFastBufferInvoke: symbol;
}
