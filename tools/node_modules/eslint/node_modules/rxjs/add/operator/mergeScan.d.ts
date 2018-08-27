import { mergeScan } from '../../operator/mergeScan';
declare module '../../Observable' {
    interface Observable<T> {
        mergeScan: typeof mergeScan;
    }
}
