
import { Observable } from '../../Observable';
import { bufferTime } from '../../operator/bufferTime';

Observable.prototype.bufferTime = bufferTime;

declare module '../../Observable' {
  interface Observable<T> {
    bufferTime: typeof bufferTime;
  }
}