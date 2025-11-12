export interface AsyncContextFrameBinding {
  getContinuationPreservedEmbedderData(): unknown,
  setContinuationPreservedEmbedderData(frame: unknown): void,
}
