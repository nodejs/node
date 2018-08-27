import { Observable } from '../../Observable';
import { auditTime } from '../../operator/auditTime';

Observable.prototype.auditTime = auditTime;

declare module '../../Observable' {
  interface Observable<T> {
    auditTime: typeof auditTime;
  }
}