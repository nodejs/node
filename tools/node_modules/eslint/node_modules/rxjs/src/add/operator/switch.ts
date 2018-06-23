
import { Observable } from '../../Observable';
import { _switch } from '../../operator/switch';

Observable.prototype.switch = _switch;
Observable.prototype._switch = _switch;

declare module '../../Observable' {
  interface Observable<T> {
    switch: typeof _switch;
    _switch: typeof _switch;
  }
}