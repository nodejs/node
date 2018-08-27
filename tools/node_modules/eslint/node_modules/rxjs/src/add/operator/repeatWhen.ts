
import { Observable } from '../../Observable';
import { repeatWhen } from '../../operator/repeatWhen';

Observable.prototype.repeatWhen = repeatWhen;

declare module '../../Observable' {
  interface Observable<T> {
    repeatWhen: typeof repeatWhen;
  }
}