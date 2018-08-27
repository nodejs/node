import { race } from '../../operator/race';
declare module '../../Observable' {
    interface Observable<T> {
        race: typeof race;
    }
}
