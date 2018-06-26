import { fromEvent as staticFromEvent } from '../../observable/fromEvent';
declare module '../../Observable' {
    namespace Observable {
        let fromEvent: typeof staticFromEvent;
    }
}
