import { AsyncWrapBinding } from './internalBinding/async_wrap';
import { BlobBinding } from './internalBinding/blob';
import { ConfigBinding } from './internalBinding/config';
import { ConstantsBinding } from './internalBinding/constants';
import { DebugBinding } from './internalBinding/debug';
import { HttpParserBinding } from './internalBinding/http_parser';
import { FsBinding } from './internalBinding/fs';
import { FsDirBinding } from './internalBinding/fs_dir';
import { MessagingBinding } from './internalBinding/messaging';
import { OptionsBinding } from './internalBinding/options';
import { OSBinding } from './internalBinding/os';
import { ProcessBinding } from './internalBinding/process';
import { SerdesBinding } from './internalBinding/serdes';
import { SymbolsBinding } from './internalBinding/symbols';
import { TimersBinding } from './internalBinding/timers';
import { TypesBinding } from './internalBinding/types';
import { URLBinding } from './internalBinding/url';
import { URLPatternBinding } from "./internalBinding/url_pattern";
import { UtilBinding } from './internalBinding/util';
import { WASIBinding } from './internalBinding/wasi';
import { WorkerBinding } from './internalBinding/worker';
import { ModulesBinding } from './internalBinding/modules';
import { ZlibBinding } from './internalBinding/zlib';

interface InternalBindingMap {
  async_wrap: AsyncWrapBinding;
  blob: BlobBinding;
  config: ConfigBinding;
  constants: ConstantsBinding;
  debug: DebugBinding;
  fs: FsBinding;
  fs_dir: FsDirBinding;
  http_parser: HttpParserBinding;
  messaging: MessagingBinding;
  modules: ModulesBinding;
  options: OptionsBinding;
  os: OSBinding;
  process: ProcessBinding;
  serdes: SerdesBinding;
  symbols: SymbolsBinding;
  timers: TimersBinding;
  types: TypesBinding;
  url: URLBinding;
  url_pattern: URLPatternBinding;
  util: UtilBinding;
  wasi: WASIBinding;
  worker: WorkerBinding;
  zlib: ZlibBinding;
}

type InternalBindingKeys = keyof InternalBindingMap;

declare function internalBinding<T extends InternalBindingKeys>(binding: T): InternalBindingMap[T]

declare global {
  type TypedArray =
    | Uint8Array
    | Uint8ClampedArray
    | Uint16Array
    | Uint32Array
    | Int8Array
    | Int16Array
    | Int32Array
    | Float32Array
    | Float64Array
    | BigUint64Array
    | BigInt64Array;

  namespace NodeJS {
    interface Global {
      internalBinding<T extends InternalBindingKeys>(binding: T): InternalBindingMap[T]
    }
  }
}
