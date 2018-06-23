
import { Observable } from '../../Observable';
import { sequenceEqual } from '../../operator/sequenceEqual';

Observable.prototype.sequenceEqual = sequenceEqual;

declare module '../../Observable' {
  interface Observable<T> {
    sequenceEqual: typeof sequenceEqual;
  }
}