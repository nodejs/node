import { ReplaySubject } from '../ReplaySubject';
/**
 * @method shareReplay
 * @owner Observable
 */
export function shareReplay(bufferSize, windowTime, scheduler) {
    return (source) => source.lift(shareReplayOperator(bufferSize, windowTime, scheduler));
}
function shareReplayOperator(bufferSize, windowTime, scheduler) {
    let subject;
    let refCount = 0;
    let subscription;
    let hasError = false;
    let isComplete = false;
    return function shareReplayOperation(source) {
        refCount++;
        if (!subject || hasError) {
            hasError = false;
            subject = new ReplaySubject(bufferSize, windowTime, scheduler);
            subscription = source.subscribe({
                next(value) { subject.next(value); },
                error(err) {
                    hasError = true;
                    subject.error(err);
                },
                complete() {
                    isComplete = true;
                    subject.complete();
                },
            });
        }
        const innerSub = subject.subscribe(this);
        return () => {
            refCount--;
            innerSub.unsubscribe();
            if (subscription && refCount === 0 && isComplete) {
                subscription.unsubscribe();
            }
        };
    };
}
;
//# sourceMappingURL=shareReplay.js.map