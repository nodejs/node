
import { Observable } from '../../Observable';
import { last } from '../../operator/last';

Observable.prototype.last = <any>last;

declare module '../../Observable' {
  interface Observable<T> {
    last: typeof last;
  }
}