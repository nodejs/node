
import { Observable } from '../../Observable';
import { _do } from '../../operator/do';

Observable.prototype.do = _do;
Observable.prototype._do = _do;

declare module '../../Observable' {
  interface Observable<T> {
    do: typeof _do;
    _do: typeof _do;
  }
}