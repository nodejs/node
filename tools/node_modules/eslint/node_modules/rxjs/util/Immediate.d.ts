export declare class ImmediateDefinition {
    private root;
    setImmediate: (cb: () => void) => number;
    clearImmediate: (handle: number) => void;
    private identify(o);
    tasksByHandle: any;
    nextHandle: number;
    currentlyRunningATask: boolean;
    constructor(root: any);
    canUseProcessNextTick(): boolean;
    canUseMessageChannel(): boolean;
    canUseReadyStateChange(): boolean;
    canUsePostMessage(): boolean;
    partiallyApplied(handler: any, ...args: any[]): () => void;
    addFromSetImmediateArguments(args: any[]): number;
    createProcessNextTickSetImmediate(): () => any;
    createPostMessageSetImmediate(): () => any;
    runIfPresent(handle: any): void;
    createMessageChannelSetImmediate(): () => any;
    createReadyStateChangeSetImmediate(): () => any;
    createSetTimeoutSetImmediate(): () => any;
}
export declare const Immediate: ImmediateDefinition;
