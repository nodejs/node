
import { Observable } from '../../Observable';
import { find } from '../../operator/find';

Observable.prototype.find = find;

declare module '../../Observable' {
  interface Observable<T> {
    find: typeof find;
  }
}