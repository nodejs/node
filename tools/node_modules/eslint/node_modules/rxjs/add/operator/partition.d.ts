import { partition } from '../../operator/partition';
declare module '../../Observable' {
    interface Observable<T> {
        partition: typeof partition;
    }
}
