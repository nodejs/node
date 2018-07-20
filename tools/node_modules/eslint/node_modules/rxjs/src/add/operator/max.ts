
import { Observable } from '../../Observable';
import { max } from '../../operator/max';

Observable.prototype.max = max;

declare module '../../Observable' {
  interface Observable<T> {
    max: typeof max;
  }
}