namespace Mathematics {
    export enum Operation {
        Add,
        Subtract,
        Multiply,
        Divide
    }

    export class Calculator {
        constructor(private op: Operation) { }

        perform(a: number, b: number): number {
            switch (this.op) {
                case Operation.Add: return a + b;
                case Operation.Subtract: return a - b;
                case Operation.Multiply: return a * b;
                case Operation.Divide:
                    if (b === 0) throw new Error("Division by zero!");
                    return a / b;
                default:
                    throw new Error("Unknown operation");
            }
        }
    }
}

throw new Error("Stacktrace at line 27");
