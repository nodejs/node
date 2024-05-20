import Dispatcher from "./dispatcher";
import RetryHandler from "./retry-handler";

export type DumpInterceptorOpts = { maxSize?: number }
export type RetryInterceptorOpts = RetryHandler.RetryOptions
export type RedirectInterceptorOpts = { maxRedirections?: number }

export declare function createRedirectInterceptor (opts: RedirectInterceptorOpts): Dispatcher.DispatcherComposeInterceptor
export declare function dump(opts?: DumpInterceptorOpts): Dispatcher.DispatcherComposeInterceptor
export declare function retry(opts?: RetryInterceptorOpts): Dispatcher.DispatcherComposeInterceptor
export declare function redirect(opts?: RedirectInterceptorOpts): Dispatcher.DispatcherComposeInterceptor
