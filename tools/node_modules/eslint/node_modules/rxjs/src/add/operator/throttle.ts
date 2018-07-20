
import { Observable } from '../../Observable';
import { throttle } from '../../operator/throttle';

Observable.prototype.throttle = throttle;

declare module '../../Observable' {
  interface Observable<T> {
    throttle: typeof throttle;
  }
}