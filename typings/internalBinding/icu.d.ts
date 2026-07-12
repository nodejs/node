export interface ICUBinding {
  Converter: object;
  decode(
    converter: object,
    input: ArrayBufferView | ArrayBuffer | SharedArrayBuffer,
    flags: number,
    fromEncoding: string,
  ): string;
  getConverter(label: string, flags: number): object | undefined;
  getStringWidth(value: string, ambiguousAsFullWidth?: boolean, expandEmojiSequence?: boolean): number;
  hasConverter(label: string): boolean;
  icuErrName(status: number): string;
  transcode(
    input: ArrayBufferView | ArrayBuffer | SharedArrayBuffer,
    fromEncoding: string,
    toEncoding: string,
  ): Buffer | number;
}
