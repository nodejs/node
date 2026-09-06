import { AsyncContextFrameBinding } from './internalBinding/async_context_frame';
import { AsyncWrapBinding } from './internalBinding/async_wrap';
import { BlobBinding } from './internalBinding/blob';
import { BufferBinding } from './internalBinding/buffer';
import { CJSLexerBinding } from './internalBinding/cjs_lexer';
import { ConfigBinding } from './internalBinding/config';
import { ConstantsBinding } from './internalBinding/constants';
import { CredentialsBinding } from './internalBinding/credentials';
import { CryptoBinding } from './internalBinding/crypto';
import { DebugBinding } from './internalBinding/debug';
import { DiagnosticsChannelBinding } from './internalBinding/diagnostics_channel';
import { EncodingBinding } from './internalBinding/encoding_binding';
import { FFIBinding } from './internalBinding/ffi';
import { FsBinding } from './internalBinding/fs';
import { FsDirBinding } from './internalBinding/fs_dir';
import { HeapUtilsBinding } from './internalBinding/heap_utils';
import { HttpParserBinding } from './internalBinding/http_parser';
import { ICUBinding } from './internalBinding/icu';
import { InspectorBinding } from './internalBinding/inspector';
import { InternalOnlyV8Binding } from './internalBinding/internal_only_v8';
import { IPCSerdesBinding } from './internalBinding/ipc_serdes';
import { LocksBinding } from './internalBinding/locks';
import { MessagingBinding } from './internalBinding/messaging';
import { ModulesBinding } from './internalBinding/modules';
import { OptionsBinding } from './internalBinding/options';
import { OSBinding } from './internalBinding/os';
import { PerformanceBinding } from './internalBinding/performance';
import { PermissionBinding } from './internalBinding/permission';
import { ProcessBinding } from './internalBinding/process';
import { ProcessWrapBinding } from './internalBinding/process_wrap';
import { ProfilerBinding } from './internalBinding/profiler';
import { SeaBinding } from './internalBinding/sea';
import { SerdesBinding } from './internalBinding/serdes';
import { SignalWrapBinding } from './internalBinding/signal_wrap';
import { StringDecoderBinding } from './internalBinding/string_decoder';
import { SymbolsBinding } from './internalBinding/symbols';
import { TimersBinding } from './internalBinding/timers';
import { TypesBinding } from './internalBinding/types';
import { URLBinding } from './internalBinding/url';
import { URLPatternBinding } from "./internalBinding/url_pattern";
import { UtilBinding } from './internalBinding/util';
import { UVBinding } from './internalBinding/uv';
import { WASIBinding } from './internalBinding/wasi';
import { WatchdogBinding } from './internalBinding/watchdog';
import { WorkerBinding } from './internalBinding/worker';
import { ZlibBinding } from './internalBinding/zlib';

interface InternalBindingMap {
  async_context_frame: AsyncContextFrameBinding;
  async_wrap: AsyncWrapBinding;
  blob: BlobBinding;
  buffer: BufferBinding;
  cjs_lexer: CJSLexerBinding;
  config: ConfigBinding;
  constants: ConstantsBinding;
  credentials: CredentialsBinding;
  crypto: CryptoBinding;
  debug: DebugBinding;
  diagnostics_channel: DiagnosticsChannelBinding;
  encoding_binding: EncodingBinding;
  ffi: FFIBinding;
  fs: FsBinding;
  fs_dir: FsDirBinding;
  heap_utils: HeapUtilsBinding;
  http_parser: HttpParserBinding;
  icu: ICUBinding;
  inspector: InspectorBinding;
  internal_only_v8: InternalOnlyV8Binding;
  ipc_serdes: IPCSerdesBinding;
  locks: LocksBinding;
  messaging: MessagingBinding;
  modules: ModulesBinding;
  options: OptionsBinding;
  os: OSBinding;
  performance: PerformanceBinding;
  permission: PermissionBinding;
  process: ProcessBinding;
  process_wrap: ProcessWrapBinding;
  profiler: ProfilerBinding;
  sea: SeaBinding;
  serdes: SerdesBinding;
  signal_wrap: SignalWrapBinding;
  string_decoder: StringDecoderBinding;
  symbols: SymbolsBinding;
  timers: TimersBinding;
  types: TypesBinding;
  url: URLBinding;
  url_pattern: URLPatternBinding;
  util: UtilBinding;
  uv: UVBinding;
  wasi: WASIBinding;
  watchdog: WatchdogBinding;
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
    | Float16Array
    | Float32Array
    | Float64Array
    | BigUint64Array
    | BigInt64Array;

  type TypedArrayConstructor =
    | typeof Uint8Array
    | typeof Uint8ClampedArray
    | typeof Uint16Array
    | typeof Uint32Array
    | typeof Int8Array
    | typeof Int16Array
    | typeof Int32Array
    | typeof Float16Array
    | typeof Float32Array
    | typeof Float64Array
    | typeof BigUint64Array
    | typeof BigInt64Array;

  namespace NodeJS {
    interface Global {
      internalBinding<T extends InternalBindingKeys>(binding: T): InternalBindingMap[T]
    }
  }
}
