"use strict";
Object.defineProperty(exports, "__esModule", { value: true });
exports.Queue = void 0;
/**
 * This is a custom stateless queue to track concurrent async fs calls.
 * It increments a counter whenever a call is queued and decrements it
 * as soon as it completes. When the counter hits 0, it calls onQueueEmpty.
 */
class Queue {
    onQueueEmpty;
    count = 0;
    constructor(onQueueEmpty) {
        this.onQueueEmpty = onQueueEmpty;
    }
    enqueue() {
        this.count++;
    }
    dequeue(error, output) {
        if (--this.count <= 0 || error)
            this.onQueueEmpty(error, output);
    }
}
exports.Queue = Queue;
