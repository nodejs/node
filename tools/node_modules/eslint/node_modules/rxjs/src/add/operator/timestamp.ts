import { Observable } from '../../Observable';
import { timestamp } from '../../operator/timestamp';

Observable.prototype.timestamp = timestamp;

declare module '../../Observable' {
  interface Observable<T> {
    timestamp: typeof timestamp;
  }
}