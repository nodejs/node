export interface HeapUtilsBinding {
  createHeapSnapshotStream(options: string[]): unknown;
  triggerHeapSnapshot(filename: string, options: string[]): string;
}
