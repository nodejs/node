import { range as staticRange } from '../../observable/range';
declare module '../../Observable' {
    namespace Observable {
        let range: typeof staticRange;
    }
}
