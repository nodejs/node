
import { Observable } from '../../Observable';
import { timeInterval } from '../../operator/timeInterval';

Observable.prototype.timeInterval = timeInterval;

declare module '../../Observable' {
  interface Observable<T> {
    timeInterval: typeof timeInterval;
  }
}