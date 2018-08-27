import { zipProto } from '../../operator/zip';
declare module '../../Observable' {
    interface Observable<T> {
        zip: typeof zipProto;
    }
}
