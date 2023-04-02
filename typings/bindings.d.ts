import type { AsyncWrapBinding } from './internalBinding/async_wrap';
import type { BlobBinding } from "./internalBinding/blob";
import type { ConfigBinding } from "./internalBinding/config";
import type { ConstantsBinding } from "./internalBinding/constants";
import type { FSBinding } from "./internalBinding/fs";
import type { HttpParserBinding } from "./internalBinding/http_parser";
import type { MessagingBinding } from "./internalBinding/messaging";
import type { OptionsBinding } from "./internalBinding/options";
import type { OSBinding } from "./internalBinding/os";
import type { SerdesBinding } from "./internalBinding/serdes";
import type { SymbolsBinding } from "./internalBinding/symbols";
import type { TimersBinding } from "./internalBinding/timers";
import type { TypesBinding } from "./internalBinding/types";
import type { URLBinding } from "./internalBinding/url";
import type { UtilBindings } from "./internalBinding/util";
import type { WorkerBinding } from "./internalBinding/worker";

export type InternalBindingMap = {
  async_wrap: AsyncWrapBinding;
  blob: BlobBinding;
  config: ConfigBinding;
  constants: ConstantsBinding;
  fs: FSBinding;
  http_parser: HttpParserBinding;
  messaging: MessagingBinding;
  options: OptionsBinding;
  os: OSBinding;
  serdes: SerdesBinding;
  symbols: SymbolsBinding;
  timers: TimersBinding;
  types: TypesBinding;
  url: URLBinding;
  util: UtilBindings;
  worker: WorkerBinding;
}

export type InternalBinding<T extends keyof InternalBindingMap> = (binding: T) => InternalBindingMap[T];
