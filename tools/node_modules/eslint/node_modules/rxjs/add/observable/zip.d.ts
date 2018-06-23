import { zip as zipStatic } from '../../observable/zip';
declare module '../../Observable' {
    namespace Observable {
        let zip: typeof zipStatic;
    }
}
