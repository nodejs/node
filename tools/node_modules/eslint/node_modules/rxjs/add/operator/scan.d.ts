import { scan } from '../../operator/scan';
declare module '../../Observable' {
    interface Observable<T> {
        scan: typeof scan;
    }
}
