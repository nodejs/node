declare namespace InternalSerdesBinding {
  type Buffer = Uint8Array;

  class Serializer {
    _getDataCloneError: typeof Error;
    constructor();
    _setTreatArrayBufferViewsAsHostObjects(value: boolean): void;
    releaseBuffer(): Buffer;
    transferArrayBuffer(id: number, arrayBuffer: ArrayBuffer): void;
    writeDouble(value: number): void;
    writeHeader(): void;
    writeRawBytes(value: ArrayBufferView): void;
    writeUint32(value: number): void;
    writeUint64(hi: number, lo: number): void;
    writeValue(value: any): void;
  }

  class Deserializer {
    buffer: ArrayBufferView;
    constructor(buffer: ArrayBufferView);
    _readRawBytes(length: number): number;
    getWireFormatVersion(): number;
    readDouble(): number;
    readHeader(): boolean;
    readRawBytes(length: number): Buffer;
    readUint32(): number;
    readUint64(): [hi: number, lo: number];
    readValue(): unknown;
    transferArrayBuffer(id: number, arrayBuffer: ArrayBuffer | SharedArrayBuffer): void;
  }
}

export interface SerdesBinding {
  Serializer: typeof InternalSerdesBinding.Serializer;
  Deserializer: typeof InternalSerdesBinding.Deserializer;
}
