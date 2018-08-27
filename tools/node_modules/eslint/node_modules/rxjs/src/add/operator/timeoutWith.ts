
import { Observable } from '../../Observable';
import { timeoutWith } from '../../operator/timeoutWith';

Observable.prototype.timeoutWith = timeoutWith;

declare module '../../Observable' {
  interface Observable<T> {
    timeoutWith: typeof timeoutWith;
  }
}