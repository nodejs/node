import { dematerialize } from '../../operator/dematerialize';
declare module '../../Observable' {
    interface Observable<T> {
        dematerialize: typeof dematerialize;
    }
}
