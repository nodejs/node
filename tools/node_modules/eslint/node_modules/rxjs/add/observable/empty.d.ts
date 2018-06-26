import { empty as staticEmpty } from '../../observable/empty';
declare module '../../Observable' {
    namespace Observable {
        let empty: typeof staticEmpty;
    }
}
