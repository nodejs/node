declare namespace InternalIPCSerdesBinding {
  type Buffer = Uint8Array;
}

export interface IPCSerdesBinding {
  serialize(message: unknown): InternalIPCSerdesBinding.Buffer;
  deserialize(buffer: ArrayBufferView): unknown;
}
