
import { Observable } from '../../Observable';
import { every } from '../../operator/every';

Observable.prototype.every = every;

declare module '../../Observable' {
  interface Observable<T> {
    every: typeof every;
  }
}