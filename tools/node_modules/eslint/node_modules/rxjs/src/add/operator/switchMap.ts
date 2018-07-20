
import { Observable } from '../../Observable';
import { switchMap } from '../../operator/switchMap';

Observable.prototype.switchMap = switchMap;

declare module '../../Observable' {
  interface Observable<T> {
    switchMap: typeof switchMap;
  }
}