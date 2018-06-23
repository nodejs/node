
import { Observable } from '../../Observable';
import { publishBehavior } from '../../operator/publishBehavior';

Observable.prototype.publishBehavior = publishBehavior;

declare module '../../Observable' {
  interface Observable<T> {
    publishBehavior: typeof publishBehavior;
  }
}