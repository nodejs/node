
import { Observable } from '../../Observable';
import { takeWhile } from '../../operator/takeWhile';

Observable.prototype.takeWhile = takeWhile;

declare module '../../Observable' {
  interface Observable<T> {
    takeWhile: typeof takeWhile;
  }
}