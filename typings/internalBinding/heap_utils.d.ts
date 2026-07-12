import { owner_symbol } from './symbols';

declare namespace InternalHeapUtilsBinding {
  interface HeapSnapshotStreamHandle {
    [owner_symbol]?: object;
    onread?: (arrayBuffer: ArrayBuffer | undefined) => void;
    readStart(): number;
  }
}

export interface HeapUtilsBinding {
  buildEmbedderGraph(): object[];
  triggerHeapSnapshot(filename: string | undefined, options: Uint8Array): string;
  createHeapSnapshotStream(
    options: Uint8Array,
  ): InternalHeapUtilsBinding.HeapSnapshotStreamHandle;
}
