
import { Observable } from '../../Observable';
import { delayWhen } from '../../operator/delayWhen';

Observable.prototype.delayWhen = delayWhen;

declare module '../../Observable' {
  interface Observable<T> {
    delayWhen: typeof delayWhen;
  }
}