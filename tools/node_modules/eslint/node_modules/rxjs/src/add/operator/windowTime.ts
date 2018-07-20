
import { Observable } from '../../Observable';
import { windowTime } from '../../operator/windowTime';

Observable.prototype.windowTime = windowTime;

declare module '../../Observable' {
  interface Observable<T> {
    windowTime: typeof windowTime;
  }
}