
import { Observable } from '../../Observable';
import { reduce } from '../../operator/reduce';

Observable.prototype.reduce = reduce;

declare module '../../Observable' {
  interface Observable<T> {
    reduce: typeof reduce;
  }
}