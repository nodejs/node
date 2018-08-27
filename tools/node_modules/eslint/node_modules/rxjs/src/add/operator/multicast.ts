
import { Observable } from '../../Observable';
import { multicast } from '../../operator/multicast';

Observable.prototype.multicast = <any>multicast;

declare module '../../Observable' {
  interface Observable<T> {
    multicast: typeof multicast;
  }
}