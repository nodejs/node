import {DispatchInterceptor} from "./dispatcher";

type RedirectInterceptorOpts = { maxRedirections?: number }

export declare function createRedirectInterceptor (opts: RedirectInterceptorOpts): DispatchInterceptor
