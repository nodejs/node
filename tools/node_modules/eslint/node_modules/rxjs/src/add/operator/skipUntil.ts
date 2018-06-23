
import { Observable } from '../../Observable';
import { skipUntil } from '../../operator/skipUntil';

Observable.prototype.skipUntil = skipUntil;

declare module '../../Observable' {
  interface Observable<T> {
    skipUntil: typeof skipUntil;
  }
}