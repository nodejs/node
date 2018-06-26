
import { Observable } from '../../Observable';
import { skipWhile } from '../../operator/skipWhile';

Observable.prototype.skipWhile = skipWhile;

declare module '../../Observable' {
  interface Observable<T> {
    skipWhile: typeof skipWhile;
  }
}