
import { Observable } from '../../Observable';
import { mergeMapTo } from '../../operator/mergeMapTo';

Observable.prototype.flatMapTo = <any>mergeMapTo;
Observable.prototype.mergeMapTo = <any>mergeMapTo;

declare module '../../Observable' {
  interface Observable<T> {
    flatMapTo: typeof mergeMapTo;
    mergeMapTo: typeof mergeMapTo;
  }
}