import { map } from '../../operator/map';
declare module '../../Observable' {
    interface Observable<T> {
        map: typeof map;
    }
}
