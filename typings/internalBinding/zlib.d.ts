declare namespace InternalZlibBinding {
  class ZlibBase {
    // These attributes are not used by the C++ binding, but declared on JS side.
    buffer?: TypedArray;
    cb?: VoidFunction;
    availOutBefore?: number;
    availInBefore?: number;
    inOff?: number;
    flushFlag?: number;

    reset(): void;
    close(): void;
    params(level: number, strategy: number): void;
    write(flushFlag: number, input: TypedArray, inputOff: number, inputLen: number, out: TypedArray, outOff: number, outLen: number): void;
    writeSync(flushFlag: number, input: TypedArray, inputOff: number, inputLen: number, out: TypedArray, outOff: number, outLen: number): void;
  }

  class Zlib extends ZlibBase {
    constructor(mode: number)
    init(windowBits: number, level: number, memLevel: number, strategy: number, writeState: Uint32Array, callback: VoidFunction, dictionary: Uint32Array): number;
  }

  class BrotliDecoder extends ZlibBase {
    constructor(mode: number);
    init(initParamsArray: Uint32Array, writeState: Uint32Array, callback: VoidFunction): boolean;
  }

  class BrotliEncoder extends ZlibBase {
    constructor(mode: number);
    init(initParamsArray: Uint32Array, writeState: Uint32Array, callback: VoidFunction): boolean;
  }

  class ZstdCompress extends ZlibBase {
    constructor();
    init(initParamsArray: Uint32Array, pledgedSrcSize: number | undefined, writeState: Uint32Array, callback: VoidFunction, dictionary?: ArrayBufferView): void;
  }

  class ZstdDecompress extends ZlibBase {
    constructor();
    init(initParamsArray: Uint32Array, pledgedSrcSize: number | undefined, writeState: Uint32Array, callback: VoidFunction, dictionary?: ArrayBufferView): void;
  }
}

export interface ZlibBinding {
  BrotliDecoder: typeof InternalZlibBinding.BrotliDecoder;
  BrotliEncoder: typeof InternalZlibBinding.BrotliEncoder;
  ZLIB_VERSION: string;
  Zlib: typeof InternalZlibBinding.Zlib;
  ZstdCompress: typeof InternalZlibBinding.ZstdCompress;
  ZstdDecompress: typeof InternalZlibBinding.ZstdDecompress;
  crc32(data: string | ArrayBufferView, value: number): number;
}
