
import { Observable } from '../../Observable';
import { filter } from '../../operator/filter';

Observable.prototype.filter = filter;

declare module '../../Observable' {
  interface Observable<T> {
    filter: typeof filter;
  }
}