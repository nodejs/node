export declare class RequestAnimationFrameDefinition {
    cancelAnimationFrame: (handle: number) => void;
    requestAnimationFrame: (cb: () => void) => number;
    constructor(root: any);
}
export declare const AnimationFrame: RequestAnimationFrameDefinition;
