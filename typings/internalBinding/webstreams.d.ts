export interface WebstreamsBinding {
  isNonThenable(value: unknown): boolean;
  cloneAsUint8Array(view: ArrayBufferView): Uint8Array;
}
