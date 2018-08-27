import { Subject } from '../Subject';
import { Observable } from '../Observable';
import { Subscriber } from '../Subscriber';
import { Subscription } from '../Subscription';
/**
 * @class ConnectableObservable<T>
 */
export declare class ConnectableObservable<T> extends Observable<T> {
    /** @deprecated internal use only */ source: Observable<T>;
    /** @deprecated internal use only */ subjectFactory: () => Subject<T>;
    /** @deprecated internal use only */ _subject: Subject<T>;
    /** @deprecated internal use only */ _refCount: number;
    /** @deprecated internal use only */ _connection: Subscription;
    _isComplete: boolean;
    constructor(/** @deprecated internal use only */ source: Observable<T>, 
        /** @deprecated internal use only */ subjectFactory: () => Subject<T>);
    /** @deprecated internal use only */ _subscribe(subscriber: Subscriber<T>): Subscription;
    /** @deprecated internal use only */ getSubject(): Subject<T>;
    connect(): Subscription;
    refCount(): Observable<T>;
}
export declare const connectableObservableDescriptor: PropertyDescriptorMap;
