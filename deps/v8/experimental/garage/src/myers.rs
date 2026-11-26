// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Myers `O(ND)` shortest-edit-script sequence aligner.
//!
//! Drop-in replacement for the `similar::{Algorithm, DiffOp,
//! capture_diff_slices}` slice aligner used by [`crate::diff`].

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Algorithm {
    Myers,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DiffOp {
    Equal {
        old_index: usize,
        new_index: usize,
        len: usize,
    },
    Delete {
        old_index: usize,
        old_len: usize,
        new_index: usize,
    },
    Insert {
        old_index: usize,
        new_index: usize,
        new_len: usize,
    },
    Replace {
        old_index: usize,
        old_len: usize,
        new_index: usize,
        new_len: usize,
    },
}

#[derive(Clone, Copy, PartialEq, Eq)]
enum RawStep {
    Equal,
    Delete,
    Insert,
}

/// Computes the Myers diff between `old` and `new` and coalesces adjacent
/// `Delete` + `Insert` runs into `DiffOp::Replace` (matching `similar`).
pub fn capture_diff_slices<T: PartialEq>(_alg: Algorithm, old: &[T], new: &[T]) -> Vec<DiffOp> {
    if old.is_empty() && new.is_empty() {
        return Vec::new();
    }
    if old.is_empty() {
        return vec![DiffOp::Insert {
            old_index: 0,
            new_index: 0,
            new_len: new.len(),
        }];
    }
    if new.is_empty() {
        return vec![DiffOp::Delete {
            old_index: 0,
            old_len: old.len(),
            new_index: 0,
        }];
    }

    // Trim shared prefix and suffix before running Myers so identical
    // regions cost `O(N)` instead of `O(ND)`.
    let mut prefix = 0;
    while prefix < old.len() && prefix < new.len() && old[prefix] == new[prefix] {
        prefix += 1;
    }
    let mut old_end = old.len();
    let mut new_end = new.len();
    while old_end > prefix && new_end > prefix && old[old_end - 1] == new[new_end - 1] {
        old_end -= 1;
        new_end -= 1;
    }

    let steps = myers_steps(&old[prefix..old_end], &new[prefix..new_end]);
    let mut ops = Vec::new();
    let mut oi = 0usize;
    let mut ni = 0usize;

    if prefix > 0 {
        ops.push(DiffOp::Equal {
            old_index: 0,
            new_index: 0,
            len: prefix,
        });
        oi = prefix;
        ni = prefix;
    }

    let mut idx = 0;
    while idx < steps.len() {
        let step = steps[idx];
        let mut count = 1;
        while idx + count < steps.len() && steps[idx + count] == step {
            count += 1;
        }
        idx += count;

        match step {
            RawStep::Equal => {
                push_op(
                    &mut ops,
                    DiffOp::Equal {
                        old_index: oi,
                        new_index: ni,
                        len: count,
                    },
                );
                oi += count;
                ni += count;
            }
            RawStep::Delete => {
                push_op(
                    &mut ops,
                    DiffOp::Delete {
                        old_index: oi,
                        old_len: count,
                        new_index: ni,
                    },
                );
                oi += count;
            }
            RawStep::Insert => {
                push_op(
                    &mut ops,
                    DiffOp::Insert {
                        old_index: oi,
                        new_index: ni,
                        new_len: count,
                    },
                );
                ni += count;
            }
        }
    }

    let suffix = old.len() - old_end;
    if suffix > 0 {
        push_op(
            &mut ops,
            DiffOp::Equal {
                old_index: oi,
                new_index: ni,
                len: suffix,
            },
        );
    }

    ops
}

fn push_op(ops: &mut Vec<DiffOp>, next: DiffOp) {
    match (ops.last().copied(), next) {
        (
            Some(DiffOp::Equal {
                old_index,
                new_index,
                len,
            }),
            DiffOp::Equal {
                old_index: o2,
                new_index: n2,
                len: l2,
            },
        ) if old_index + len == o2 && new_index + len == n2 => {
            *ops.last_mut().unwrap() = DiffOp::Equal {
                old_index,
                new_index,
                len: len + l2,
            };
        }
        (
            Some(DiffOp::Delete {
                old_index, old_len, ..
            }),
            DiffOp::Insert {
                new_index, new_len, ..
            },
        )
        | (
            Some(DiffOp::Insert {
                new_index, new_len, ..
            }),
            DiffOp::Delete {
                old_index, old_len, ..
            },
        ) => {
            *ops.last_mut().unwrap() = DiffOp::Replace {
                old_index,
                old_len,
                new_index,
                new_len,
            };
        }
        (
            Some(DiffOp::Replace {
                old_index,
                old_len,
                new_index,
                new_len,
            }),
            DiffOp::Delete {
                old_len: more_old, ..
            },
        ) => {
            *ops.last_mut().unwrap() = DiffOp::Replace {
                old_index,
                old_len: old_len + more_old,
                new_index,
                new_len,
            };
        }
        (
            Some(DiffOp::Replace {
                old_index,
                old_len,
                new_index,
                new_len,
            }),
            DiffOp::Insert {
                new_len: more_new, ..
            },
        ) => {
            *ops.last_mut().unwrap() = DiffOp::Replace {
                old_index,
                old_len,
                new_index,
                new_len: new_len + more_new,
            };
        }
        _ => ops.push(next),
    }
}

/// Linear-space Hirschberg + Myers divide-and-conquer so even huge phases
/// with thousands of unmatched rows never blow memory (`O(N + M)` space).
fn myers_steps<T: PartialEq>(old: &[T], new: &[T]) -> Vec<RawStep> {
    let mut out = Vec::with_capacity(old.len() + new.len());
    hirschberg(old, new, &mut out);
    out
}

fn hirschberg<T: PartialEq>(old: &[T], new: &[T], out: &mut Vec<RawStep>) {
    let mut start = 0;
    while start < old.len() && start < new.len() && old[start] == new[start] {
        out.push(RawStep::Equal);
        start += 1;
    }
    let old = &old[start..];
    let new = &new[start..];

    let mut suffix = 0;
    while suffix < old.len()
        && suffix < new.len()
        && old[old.len() - 1 - suffix] == new[new.len() - 1 - suffix]
    {
        suffix += 1;
    }
    let core_old = &old[..old.len() - suffix];
    let core_new = &new[..new.len() - suffix];

    if core_old.is_empty() {
        out.extend(std::iter::repeat_n(RawStep::Insert, core_new.len()));
    } else if core_new.is_empty() {
        out.extend(std::iter::repeat_n(RawStep::Delete, core_old.len()));
    } else if let Some(steps) = bounded_myers_trace(core_old, core_new, 512) {
        out.extend(steps);
    } else if let Some((x, y, u, v)) = find_middle_snake(core_old, core_new) {
        hirschberg(&core_old[..x], &core_new[..y], out);
        out.extend(std::iter::repeat_n(RawStep::Equal, u - x));
        hirschberg(&core_old[u..], &core_new[v..], out);
    } else {
        out.extend(std::iter::repeat_n(RawStep::Delete, core_old.len()));
        out.extend(std::iter::repeat_n(RawStep::Insert, core_new.len()));
    }

    out.extend(std::iter::repeat_n(RawStep::Equal, suffix));
}

/// Classic forward Myers when `D <= limit` (fast path for typical phase diffs).
fn bounded_myers_trace<T: PartialEq>(old: &[T], new: &[T], limit: usize) -> Option<Vec<RawStep>> {
    let n = old.len() as isize;
    let m = new.len() as isize;
    let max_d = ((n + m) as usize).min(limit);
    let offset = max_d as isize + 1;
    let size = (2 * offset + 2) as usize;
    let mut v = vec![0isize; size];
    let mut trace: Vec<Vec<isize>> = Vec::new();

    for d in 0..=max_d as isize {
        trace.push(v.clone());
        let mut k = -d;
        while k <= d {
            let idx = (k + offset) as usize;
            let mut x = if k == -d
                || (k != d && v[(k - 1 + offset) as usize] < v[(k + 1 + offset) as usize])
            {
                v[(k + 1 + offset) as usize]
            } else {
                v[(k - 1 + offset) as usize] + 1
            };
            let mut y = x - k;
            while x < n && y < m && old[x as usize] == new[y as usize] {
                x += 1;
                y += 1;
            }
            v[idx] = x;
            if x >= n && y >= m {
                return Some(backtrack(&trace, n, m, offset));
            }
            k += 2;
        }
    }
    None
}

fn backtrack(trace: &[Vec<isize>], mut x: isize, mut y: isize, offset: isize) -> Vec<RawStep> {
    let mut rev = Vec::with_capacity((x + y) as usize);
    for d in (0..trace.len() as isize).rev() {
        let v = &trace[d as usize];
        let k = x - y;
        let prev_k =
            if k == -d || (k != d && v[(k - 1 + offset) as usize] < v[(k + 1 + offset) as usize]) {
                k + 1
            } else {
                k - 1
            };
        let prev_x = v[(prev_k + offset) as usize];
        let prev_y = prev_x - prev_k;

        while x > prev_x && y > prev_y {
            rev.push(RawStep::Equal);
            x -= 1;
            y -= 1;
        }
        if d > 0 {
            if x == prev_x {
                rev.push(RawStep::Insert);
                y -= 1;
            } else {
                rev.push(RawStep::Delete);
                x -= 1;
            }
        }
    }
    rev.reverse();
    rev
}

/// Steps one edit onto diagonal `k` at edit distance `d`, staying strictly
/// within the `[0..=n] x [0..=m]` edit graph.
fn step_on_grid(
    v: &[Option<usize>],
    k: isize,
    d: isize,
    offset: isize,
    n: usize,
    m: usize,
) -> Option<(usize, usize)> {
    if d == 0 {
        return Some((0, 0));
    }
    let ins = (k < d)
        .then(|| v[(k + 1 + offset) as usize])
        .flatten()
        .and_then(|x| {
            let y_prev = x as isize - (k + 1);
            (y_prev >= 0 && (y_prev as usize) < m).then_some((x, y_prev as usize + 1))
        });
    let del = (k > -d)
        .then(|| v[(k - 1 + offset) as usize])
        .flatten()
        .and_then(|x_prev| {
            let y = x_prev as isize - (k - 1);
            (x_prev < n && y >= 0 && (y as usize) <= m).then_some((x_prev + 1, y as usize))
        });
    match (ins, del) {
        (Some(i), Some(dl)) => Some(if dl.0 >= i.0 { dl } else { i }),
        (Some(i), None) => Some(i),
        (None, Some(dl)) => Some(dl),
        (None, None) => None,
    }
}

/// Myers 1986 §4b middle-snake finder in `O(N + M)` space for large `D`.
fn find_middle_snake<T: PartialEq>(old: &[T], new: &[T]) -> Option<(usize, usize, usize, usize)> {
    let n = old.len();
    let m = new.len();
    let max = n + m;
    if max == 0 {
        return None;
    }
    let delta = n as isize - m as isize;
    let odd = delta.rem_euclid(2) == 1;
    let half = (max as isize + 1) / 2;
    let offset = half + 2;
    let size = (2 * offset + 2) as usize;
    let mut vf: Vec<Option<usize>> = vec![None; size];
    let mut vb: Vec<Option<usize>> = vec![None; size];

    for d in 0..=half {
        let mut k = -d;
        while k <= d {
            let idx = (k + offset) as usize;
            let Some((x0, y0)) = step_on_grid(&vf, k, d, offset, n, m) else {
                vf[idx] = None;
                k += 2;
                continue;
            };
            let mut x = x0;
            let mut y = y0;
            while x < n && y < m && old[x] == new[y] {
                x += 1;
                y += 1;
            }
            vf[idx] = Some(x);

            let kb = delta - k;
            if odd
                && kb.abs() < d
                && let Some(xb) = vb[(kb + offset) as usize]
                && x + xb >= n
            {
                return Some((x0, y0, x, y));
            }
            k += 2;
        }

        let mut k = -d;
        while k <= d {
            let idx = (k + offset) as usize;
            let Some((xb0, yb0)) = step_on_grid(&vb, k, d, offset, n, m) else {
                vb[idx] = None;
                k += 2;
                continue;
            };
            let u = n - xb0;
            let v = m - yb0;
            let mut xb = xb0;
            let mut yb = yb0;
            while xb < n && yb < m && old[n - 1 - xb] == new[m - 1 - yb] {
                xb += 1;
                yb += 1;
            }
            vb[idx] = Some(xb);

            let kf = delta - k;
            if !odd
                && kf.abs() <= d
                && let Some(xf) = vf[(kf + offset) as usize]
                && xf + xb >= n
            {
                let x0 = n - xb;
                let y0 = m - yb;
                return Some((x0, y0, u, v));
            }
            k += 2;
        }
    }
    None
}

#[cfg(test)]
mod tests {
    use super::*;

    fn reconstruct(old: &[i32], new: &[i32], ops: &[DiffOp]) -> (Vec<i32>, Vec<i32>, usize) {
        let mut lo = Vec::new();
        let mut ro = Vec::new();
        let mut equal_len = 0usize;
        for op in ops {
            match *op {
                DiffOp::Equal {
                    old_index,
                    new_index,
                    len,
                } => {
                    assert_eq!(
                        &old[old_index..old_index + len],
                        &new[new_index..new_index + len]
                    );
                    lo.extend_from_slice(&old[old_index..old_index + len]);
                    ro.extend_from_slice(&new[new_index..new_index + len]);
                    equal_len += len;
                }
                DiffOp::Delete {
                    old_index, old_len, ..
                } => {
                    lo.extend_from_slice(&old[old_index..old_index + old_len]);
                }
                DiffOp::Insert {
                    new_index, new_len, ..
                } => {
                    ro.extend_from_slice(&new[new_index..new_index + new_len]);
                }
                DiffOp::Replace {
                    old_index,
                    old_len,
                    new_index,
                    new_len,
                } => {
                    lo.extend_from_slice(&old[old_index..old_index + old_len]);
                    ro.extend_from_slice(&new[new_index..new_index + new_len]);
                }
            }
        }
        (lo, ro, equal_len)
    }

    fn verify(old: &[i32], new: &[i32], expected_equal: usize) {
        let ops = capture_diff_slices(Algorithm::Myers, old, new);
        let (lo, ro, eq) = reconstruct(old, new, &ops);
        assert_eq!(lo, old);
        assert_eq!(ro, new);
        assert_eq!(eq, expected_equal);
    }

    #[test]
    fn small_and_large_diffs_round_trip() {
        let a = [1, 2, 3, 4, 5];
        let b = [1, 9, 3, 5, 6];
        verify(&a, &b, 3);

        // Exercise the Hirschberg fallback (`D > 512`).
        let big_a: Vec<i32> = (0..700).collect();
        let big_b: Vec<i32> = (350..1050).collect();
        verify(&big_a, &big_b, 350);
    }

    #[test]
    fn large_uneven_diffs_in_hirschberg_fallback_round_trip() {
        // Disjoint uneven inputs with D > 512 (even and odd delta, n < m and n > m).
        let short: Vec<i32> = (0..100).collect();
        let long_even: Vec<i32> = (1000..1450).collect();
        let long_odd: Vec<i32> = (1000..1451).collect();
        verify(&short, &long_even, 0);
        verify(&long_even, &short, 0);
        verify(&short, &long_odd, 0);
        verify(&long_odd, &short, 0);

        // Uneven inputs with a shared interior block and D > 512.
        let mut mid_short = vec![-1];
        mid_short.extend(0..80);
        mid_short.push(-2);

        let mut mid_long_front = vec![1000];
        mid_long_front.extend(0..80);
        mid_long_front.extend(2000..2600);
        verify(&mid_short, &mid_long_front, 80);
        verify(&mid_long_front, &mid_short, 80);

        let mut mid_long_back: Vec<i32> = (1000..1600).collect();
        mid_long_back.extend(0..80);
        mid_long_back.push(2000);
        verify(&mid_short, &mid_long_back, 80);
        verify(&mid_long_back, &mid_short, 80);

        // Overlapping uneven ranges with D > 512.
        let overlap_a: Vec<i32> = (0..400).collect();
        let overlap_b: Vec<i32> = (200..900).collect();
        verify(&overlap_a, &overlap_b, 200);
        verify(&overlap_b, &overlap_a, 200);

        // Interior matches near both ends separated by > 512 insertions.
        let mut split_short = vec![-1];
        split_short.extend(1..99);
        split_short.push(-2);

        let mut split_long = vec![-3];
        split_long.extend(1..90);
        split_long.extend(1000..1600);
        split_long.extend(90..99);
        split_long.push(-4);
        verify(&split_short, &split_long, 98);
        verify(&split_long, &split_short, 98);
    }
}
