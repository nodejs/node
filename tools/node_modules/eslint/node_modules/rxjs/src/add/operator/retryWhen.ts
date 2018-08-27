
import { Observable } from '../../Observable';
import { retryWhen } from '../../operator/retryWhen';

Observable.prototype.retryWhen = retryWhen;

declare module '../../Observable' {
  interface Observable<T> {
    retryWhen: typeof retryWhen;
  }
}