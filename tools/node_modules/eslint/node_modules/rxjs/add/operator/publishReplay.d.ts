import { publishReplay } from '../../operator/publishReplay';
declare module '../../Observable' {
    interface Observable<T> {
        publishReplay: typeof publishReplay;
    }
}
