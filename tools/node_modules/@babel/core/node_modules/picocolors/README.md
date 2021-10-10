# picocolors

    npm install picocolors

A tinier and faster alternative to [nanocolors](https://github.com/ai/nanocolors). Andrey, are you even trying?

```javascript
import pc from "picocolors";

console.log(pc.green(`How are ${pc.italic(`you`)} doing?`));
```

- Up to [2x faster and 2x smaller](#benchmarks) than alternatives
- 3x faster and 10x smaller than `chalk`
- [TypeScript](https://www.typescriptlang.org/) support
- [`NO_COLOR`](https://no-color.org/) friendly
- Node.js v6+ & browsers support
- The same API, but faster, much faster
- No `String.prototype` modifications (anyone still doing it?)
- No dependencies and the smallest `node_modules` footprint

## Docs
Read **[full docs](https://github.com/alexeyraspopov/picocolors#readme)** on GitHub.
