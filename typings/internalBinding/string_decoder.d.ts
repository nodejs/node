declare namespace InternalStringDecoderBinding {
  type Buffer = Uint8Array;
}

export interface StringDecoderBinding {
  readonly kIncompleteCharactersStart: number;
  readonly kIncompleteCharactersEnd: number;
  readonly kMissingBytes: number;
  readonly kBufferedBytes: number;
  readonly kEncodingField: number;
  readonly kNumFields: number;
  readonly kSize: number;

  readonly encodings: string[];

  decode(decoder: InternalStringDecoderBinding.Buffer, buffer: ArrayBufferView): string;
  flush(decoder: InternalStringDecoderBinding.Buffer): string;
}

