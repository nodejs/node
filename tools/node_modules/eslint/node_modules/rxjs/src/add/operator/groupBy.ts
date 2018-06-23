
import { Observable } from '../../Observable';
import { groupBy } from '../../operator/groupBy';

Observable.prototype.groupBy = <any>groupBy;

declare module '../../Observable' {
  interface Observable<T> {
    groupBy: typeof groupBy;
  }
}