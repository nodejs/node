import { of as staticOf } from '../../observable/of';
declare module '../../Observable' {
    namespace Observable {
        let of: typeof staticOf;
    }
}
