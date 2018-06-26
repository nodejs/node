import { auditTime } from '../../operator/auditTime';
declare module '../../Observable' {
    interface Observable<T> {
        auditTime: typeof auditTime;
    }
}
