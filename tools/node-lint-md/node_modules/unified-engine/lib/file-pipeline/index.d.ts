export const filePipeline: import('trough').Pipeline
export type Pipeline = import('trough').Pipeline
export type VFile = import('vfile').VFile
export type VFileMessage = import('vfile-message').VFileMessage
export type Node = import('unist').Node
export type Processor = import('unified').Processor
export type FileSet = import('../file-set.js').FileSet
export type Configuration = import('../configuration.js').Configuration
export type Settings = import('../index.js').Settings
export type Context = {
  processor: Processor
  fileSet: FileSet
  configuration: Configuration
  settings: Settings
  tree?: import('unist').Node<import('unist').Data> | undefined
}
