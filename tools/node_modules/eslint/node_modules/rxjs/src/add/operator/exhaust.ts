
import { Observable } from '../../Observable';
import { exhaust } from '../../operator/exhaust';

Observable.prototype.exhaust = exhaust;

declare module '../../Observable' {
  interface Observable<T> {
    exhaust: typeof exhaust;
  }
}