export const unified: import('..').FrozenProcessor<void, void, void, void>
export type Node = import('unist').Node
export type VFileCompatible = import('vfile').VFileCompatible
export type VFileValue = import('vfile').VFileValue
export type Processor = import('..').Processor
export type Plugin = import('..').Plugin
export type Preset = import('..').Preset
export type Pluggable = import('..').Pluggable
export type PluggableList = import('..').PluggableList
export type Transformer = import('..').Transformer
export type Parser = import('..').Parser
export type Compiler = import('..').Compiler
export type RunCallback = import('..').RunCallback
export type ProcessCallback = import('..').ProcessCallback
export type Context = {
  tree: Node
  file: VFile
}
import {VFile} from 'vfile'
