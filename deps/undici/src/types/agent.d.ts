import { URL } from 'url'
import Dispatcher = require('./dispatcher')
import Pool = require('./pool')

export = Agent

declare class Agent extends Dispatcher{
  constructor(opts?: Agent.Options)
  /** `true` after `dispatcher.close()` has been called. */
  closed: boolean;
  /** `true` after `dispatcher.destroyed()` has been called or `dispatcher.close()` has been called and the dispatcher shutdown has completed. */
  destroyed: boolean;
  /** Dispatches a request. */
  dispatch(options: Agent.DispatchOptions, handler: Dispatcher.DispatchHandlers): boolean;
}

declare namespace Agent {
  export interface Options extends Pool.Options {
    /** Default: `(origin, opts) => new Pool(origin, opts)`. */
    factory?(origin: URL, opts: Object): Dispatcher;
    /** Integer. Default: `0` */
    maxRedirections?: number;
  }

  export interface DispatchOptions extends Dispatcher.DispatchOptions {
    /** Integer. */
    maxRedirections?: number;
  }
}
