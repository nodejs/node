export interface BufferBinding {
  atob(input: string): string | -1 | -2 | -3;
  btoa(input: string): string | -1;

  setBufferPrototype(proto: object): void;

  byteLengthUtf8(str: string): number;
  copy(source: ArrayBufferView, target: ArrayBufferView, targetStart: number, sourceStart: number, toCopy: number): number;
  compare(a: ArrayBufferView, b: ArrayBufferView): number;
  compareOffset(source: ArrayBufferView, target: ArrayBufferView, targetStart?: number, sourceStart?: number, targetEnd?: number, sourceEnd?: number): number;
  fill(buf: ArrayBufferView, val: any, start?: number, end?: number, encoding?: number): -1 | -2 | void;
  indexOfBuffer(haystack: ArrayBufferView, needle: ArrayBufferView, offset?: number, encoding?: number, isForward?: boolean): number;
  indexOfNumber(buf: ArrayBufferView, needle: number, offset?: number, isForward?: boolean): number;
  indexOfString(buf: ArrayBufferView, needle: string, offset?: number, encoding?: number, isForward?: boolean): number;

  copyArrayBuffer(destination: ArrayBuffer | SharedArrayBuffer, destinationOffset: number, source: ArrayBuffer | SharedArrayBuffer, sourceOffset: number, bytesToCopy: number): void;

  swap16(buf: ArrayBufferView): ArrayBufferView;
  swap32(buf: ArrayBufferView): ArrayBufferView;
  swap64(buf: ArrayBufferView): ArrayBufferView;

  isUtf8(input: ArrayBufferView | ArrayBuffer | SharedArrayBuffer): boolean;
  isAscii(input: ArrayBufferView | ArrayBuffer | SharedArrayBuffer): boolean;

  kMaxLength: number;
  kStringMaxLength: number;

  asciiSlice(start: number, end: number): string;
  base64Slice(start: number, end: number): string;
  base64urlSlice(start: number, end: number): string;
  latin1Slice(start: number, end: number): string;
  hexSlice(start: number, end: number): string;
  ucs2Slice(start: number, end: number): string;
  utf8Slice(start: number, end: number): string;

  base64Write(str: string, offset?: number, maxLength?: number): number;
  base64urlWrite(str: string, offset?: number, maxLength?: number): number;
  hexWrite(str: string, offset?: number, maxLength?: number): number;
  ucs2Write(str: string, offset?: number, maxLength?: number): number;

  asciiWriteStatic(buf: ArrayBufferView, str: string, offset?: number, maxLength?: number): number;
  latin1WriteStatic(buf: ArrayBufferView, str: string, offset?: number, maxLength?: number): number;
  utf8WriteStatic(buf: ArrayBufferView, str: string, offset?: number, maxLength?: number): number;

  getZeroFillToggle(): Uint32Array | void;
}
