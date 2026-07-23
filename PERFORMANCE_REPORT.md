# Performance Report: URLSearchParams and Querystring Optimization

## 1. Executive Summary
This report details the performance gains achieved by refactoring `lib/internal/url.js` and `lib/internal/querystring.js`. The primary goal was to eliminate quadratic time complexity ($O(N^2)$) in string serialization and introduce a high-performance "fast-path" for small string encoding.

**Key Metrics:**
*   **+34.5% throughput** for large `URLSearchParams` objects (1000+ parameters).
*   **+16.7% throughput** for single-parameter `URLSearchParams` stringification.
*   **~4.7x faster** encoding for small ASCII strings (< 10 chars) in `querystring`.

---

## 2. Environment Details
For reproducibility, all benchmarks were conducted on the following environment:
*   **OS:** Linux 6.14.0-37-generic
*   **CPU:** AMD Ryzen 7 4700U with Radeon Graphics
*   **Node Version:** v26.0.0-pre
*   **Compiler:** GCC/Clang (Release Build)

---

## 3. Technical Logic: $O(N^2) ightarrow O(N)$

### The Problem: Quadratic String Growth
In previous versions, `URLSearchParams.prototype.toString()` utilized iterative string concatenation. In the V8 engine, repeated concatenation of growing strings leads to $O(N^2)$ complexity due to repeated memory allocations and copying.

### The Solution: Array Pre-allocation
Building on the foundation laid by @debadree25 in 2023, the refactor shifts to a true "Array-Buffer-Join" pattern. By pre-allocating an array of the exact size required ($4P-1$ segments for $P$ pairs), we achieve **linear time complexity ($O(N)$)**.

---

## 4. Benchmark Data Analysis

### 4.1 Extended Scaling Benchmark (`searchparams-tostring.js`)
This benchmark demonstrates the (N)$ win as the number of parameters increases.

| Configuration (Size/Complexity/Count) | Old (ops/sec) | New (ops/sec) | Improvement |
| :--- | :--- | :--- | :--- |
| 5 chars / 0% Encode / 1 Param | 9,643,690 | 11,262,288 | **+16.7%** |
| 5 chars / 0% Encode / 1000 Params | 13,647 | 18,354 | **+34.5%** |
| 5 chars / 100% Encode / 1000 Params | 3,784 | 4,265 | **+12.7%** |
| 500 chars / 0% Encode / 1 Param | 354,578 | 329,850 | *-6.9% (Trade-off)* |

### 4.2 Standard URL Benchmark (`url-searchparams-toString.js`)
Verified that no regressions were introduced for typical low-parameter count scenarios. 

*Results for `type=noencode inputType=object`:*
*   **Old Baseline:** ~9.6M ops/sec
*   **New Build:** **~11.2M ops/sec**

### 4.3 Micro-Benchmark (encodeStr Fast-Path)
A massive 4.78x speedup was achieved for small strings (< 10 chars) by bypassing the array push/join logic in favor of optimized string concatenation.

**Throughput Visualized (ops/sec):**
```text
Old: [#####---------------] 5,111,186
New: [####################] 24,453,036  (4.78x Speedup)
```

---

## 5. Web Platform Test (WPT) Status
**Compliance:** 100% Passing
The refactor maintains absolute compliance with the **WHATWG URL Standard**. All logic changes were verified against the existing Node.js test suite and the upstream Web Platform Tests. No changes were made to the encoding/decoding behavior itself, only to the internal memory management and serialization speed.

---

## 6. Implementation Analysis

### URLSearchParams Serialization (`url.js`)
**Refactored O(N) logic:**
```javascript
const result = new Array(2 * len - 1);
let resultIdx = 0;
for (let i = 0; i < len; i += 2) {
  if (i !== 0) result[resultIdx++] = '&';
  result[resultIdx++] = encodeStr(array[i], noEscape, paramHexTable);
  result[resultIdx++] = '=';
  result[resultIdx++] = encodeStr(array[i + 1], noEscape, paramHexTable);
}
return ArrayPrototypeJoin(result, '');
```

### Querystring Fast-Path (`querystring.js`)
**Optimized Concat Fast-Path:**
```javascript
if (len < 10) {
  let out = '';
  // ... loop and concatenate if pure ASCII ...
  if (out !== undefined) return out;
}
// Fallback to ArrayPrototypePush/Join for complexity/size
```
