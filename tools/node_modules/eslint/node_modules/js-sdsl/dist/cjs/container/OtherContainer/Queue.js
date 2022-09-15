"use strict";
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", { value: true });
const Deque_1 = __importDefault(require("../SequentialContainer/Deque"));
const index_1 = require("../ContainerBase/index");
class Queue extends index_1.Base {
    constructor(container = []) {
        super();
        this.queue = new Deque_1.default(container);
        this.length = this.queue.size();
    }
    clear() {
        this.queue.clear();
        this.length = 0;
    }
    /**
     * @description Inserts element to queue's end.
     */
    push(element) {
        this.queue.pushBack(element);
        this.length += 1;
    }
    /**
     * @description Removes the first element.
     */
    pop() {
        this.queue.popFront();
        if (this.length)
            this.length -= 1;
    }
    /**
     * @description Access the first element.
     */
    front() {
        return this.queue.front();
    }
}
exports.default = Queue;
