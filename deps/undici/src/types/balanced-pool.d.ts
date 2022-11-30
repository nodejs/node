import Pool from './pool'
import Dispatcher from './dispatcher'
import { URL } from 'url'

export default BalancedPool

declare class BalancedPool extends Dispatcher {
  constructor(url: string | URL | string[], options?: Pool.Options);

  addUpstream(upstream: string): BalancedPool;
  removeUpstream(upstream: string): BalancedPool;
  upstreams: Array<string>;

  /** `true` after `pool.close()` has been called. */
  closed: boolean;
  /** `true` after `pool.destroyed()` has been called or `pool.close()` has been called and the pool shutdown has completed. */
  destroyed: boolean;
}
