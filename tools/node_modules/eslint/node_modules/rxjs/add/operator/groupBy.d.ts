import { groupBy } from '../../operator/groupBy';
declare module '../../Observable' {
    interface Observable<T> {
        groupBy: typeof groupBy;
    }
}
