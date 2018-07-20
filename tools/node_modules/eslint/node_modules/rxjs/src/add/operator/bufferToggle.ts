
import { Observable } from '../../Observable';
import { bufferToggle } from '../../operator/bufferToggle';

Observable.prototype.bufferToggle = bufferToggle;

declare module '../../Observable' {
  interface Observable<T> {
    bufferToggle: typeof bufferToggle;
  }
}