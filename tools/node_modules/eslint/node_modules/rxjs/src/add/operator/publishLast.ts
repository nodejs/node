
import { Observable } from '../../Observable';
import { publishLast } from '../../operator/publishLast';

Observable.prototype.publishLast = publishLast;

declare module '../../Observable' {
  interface Observable<T> {
    publishLast: typeof publishLast;
  }
}