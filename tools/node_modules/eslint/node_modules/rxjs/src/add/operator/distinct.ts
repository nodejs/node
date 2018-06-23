import { Observable } from '../../Observable';
import { distinct } from '../../operator/distinct';

Observable.prototype.distinct = distinct;

declare module '../../Observable' {
  interface Observable<T> {
    distinct: typeof distinct;
  }
}