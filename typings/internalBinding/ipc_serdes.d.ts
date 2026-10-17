declare namespace InternalIPCSerdesBinding {
  type Buffer = Uint8Array;

  // Per-channel native framer for the `advanced` codec. Accumulates partial
  // reads in C++ and returns the complete, deserialized messages found in each
  // chunk. See src/node_ipc_serdes.cc.
  class IPCChannelFramer {
    constructor();
    read(chunk: Uint8Array): unknown[];
    buffering(): boolean;
  }
}

export interface IPCSerdesBinding {
  serialize(message: unknown): InternalIPCSerdesBinding.Buffer;
  deserialize(buffer: ArrayBufferView): unknown;
  IPCChannelFramer: typeof InternalIPCSerdesBinding.IPCChannelFramer;
}
