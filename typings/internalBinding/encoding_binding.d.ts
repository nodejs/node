export interface EncodingBinding {
  encodeInto(source: string, dest: Uint8Array): void;
  encodeUtf8String(str: string): Uint8Array;
  decodeUTF8(buffer: ArrayBufferView | ArrayBuffer | SharedArrayBuffer, ignoreBOM?: boolean, hasFatal?: boolean): string;
  toASCII(input: string): string;
  toUnicode(input: string): string;
  decodeLatin1(buffer: ArrayBufferView | ArrayBuffer | SharedArrayBuffer, ignoreBOM?: boolean, hasFatal?: boolean): string;
}