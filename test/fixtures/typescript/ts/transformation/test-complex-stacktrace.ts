// Namespace
namespace Mathematics {
    // Enum
    export enum Operation {
        Add,
        Subtract,
        Multiply,
        Divide
    }

    // Interface
    export interface MathOperation {
        perform(a: number, b: number): number;
    }

    // Generic function
    export function executeOperation<T extends MathOperation>(op: T, x: number, y: number): number {
        return op.perform(x, y);
    }

    // Class implementing interface
    export class Calculator implements MathOperation {
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

// Using the namespace
const calc = new Mathematics.Calculator(Mathematics.Operation.Divide);

// Union type
type Result = number | string;

// Function with rest parameters and arrow syntax
const processResults = (...results: Result[]): string => {
    return results.map(r => r.toString()).join(", ");
};

// Async function with await
async function performAsyncCalculation(a: number, b: number): Promise<number> {
    const result = await Promise.resolve(Mathematics.executeOperation(calc, a, b));
    return result;
}

// IIFE to create a block scope
(async () => {
    try {
        const result = await performAsyncCalculation(10, 0);
    } catch (error) {
        if (error instanceof Error) {
            // This line will throw an error
            throw new Error("Calculation failed: " + error.message);
        }
    }
})();
