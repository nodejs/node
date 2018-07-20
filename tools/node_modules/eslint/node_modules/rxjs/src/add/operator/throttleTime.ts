
import { Observable } from '../../Observable';
import { throttleTime } from '../../operator/throttleTime';

Observable.prototype.throttleTime = throttleTime;

declare module '../../Observable' {
  interface Observable<T> {
    throttleTime: typeof throttleTime;
  }
}