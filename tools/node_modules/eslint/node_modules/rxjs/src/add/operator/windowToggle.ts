
import { Observable } from '../../Observable';
import { windowToggle } from '../../operator/windowToggle';

Observable.prototype.windowToggle = windowToggle;

declare module '../../Observable' {
  interface Observable<T> {
    windowToggle: typeof windowToggle;
  }
}