import { Observable } from '../../Observable';
import { takeLast } from '../../operator/takeLast';

Observable.prototype.takeLast = takeLast;

declare module '../../Observable' {
  interface Observable<T> {
    takeLast: typeof takeLast;
  }
}