// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Non-graph listing phases: `--print-opt-code` sections and
//! `--print-bytecode` tables.
//!
//! The only structured rows here are disassembly lines,
//! `0xADDR  OFF  ENCODING  mnemonic operands…`, whose three leading columns
//! are hex on both architectures; arm64 adds `(addr 0x…)` branch targets and
//! `;; …` reloc comments, x64 prints `jna 0x… <+0x71>` and parenthesised
//! comments. Everything is recorded as spans over the emitted text — no
//! decoding beyond what V8 printed is promised (PLAN §7.6).
//!
//! Constant pools, deopt-data tables, safepoint and reloc rows are kept as
//! plain (dim) content: they are reference tables, not structure.

use std::ops::Range;

use super::maglev::line_text;
use crate::model::{LineInfo, ParsedPhase, Span};
use crate::source::LogBuffer;

pub fn parse_listing_phase(buffer: &LogBuffer, lines: Range<usize>, name: &str) -> ParsedPhase {
    // Inside an `Instructions` phase every non-blank row is expected to be a
    // disassembly row; anything else is *unrecognised* and must say so
    // (found in review: degrading to Control made a parser regression
    // invisible — rows just went dim, and the goldens could not falsify the
    // coverage claim). The table phases (pools, safepoints, reloc, deopt
    // data) are heterogeneous reference dumps, where Control is the honest
    // classification.
    let disasm_phase = name == "Instructions";
    // An Ignition dump's constant pool is its own Listing phase; its entry
    // rows resolve the `[N:…]` operands of the bytecode phase above
    //, so they get the same classification as in Maglev dumps.
    let pool_phase = name == "Constant pool";
    // The `--- Raw source ---` block is JavaScript, not trace furniture —
    // its lines get JS token styling at render time.
    let source_phase = name == "Raw source";
    let mut phase = ParsedPhase::default();
    for (offset, line) in lines.enumerate() {
        let text = line_text(buffer, line);
        let info = if offset == 0 {
            LineInfo::Banner
        } else if source_phase {
            if text.trim().is_empty() {
                LineInfo::Control
            } else {
                LineInfo::Source
            }
        } else if let Some(info) = parse_disasm_row(&text) {
            info
        } else if pool_phase && let Some(info) = super::maglev::parse_pool_entry(&text) {
            info
        } else if disasm_phase && !text.trim().is_empty() {
            phase.annotation_count += 1;
            LineInfo::Annotation { after_node: None }
        } else {
            LineInfo::Control
        };
        phase.infos.push(info);
    }

    // Label pass: every branch destination in the listing gets
    // `labeled` — the de-facto basic-block leaders the view marks and
    // `[`/`]` walk. Both target forms count; each resolves within this
    // phase only.
    let mut offsets = std::collections::HashSet::new();
    let mut addrs = std::collections::HashSet::new();
    for info in &phase.infos {
        if let LineInfo::Disasm {
            target_offset,
            target_addr,
            ..
        } = info
        {
            offsets.extend(*target_offset);
            addrs.extend(*target_addr);
        }
    }
    if !offsets.is_empty() || !addrs.is_empty() {
        for info in &mut phase.infos {
            if let LineInfo::Disasm {
                offset,
                addr,
                labeled,
                ..
            } = info
            {
                *labeled = offsets.contains(offset) || addrs.contains(addr);
            }
        }
    }
    phase
}

fn hex_run(s: &str) -> usize {
    s.bytes().take_while(|b| b.is_ascii_hexdigit()).count()
}

fn space_run(s: &str) -> usize {
    s.bytes().take_while(|b| *b == b' ').count()
}

/// `0x1700001bc    1c  540002c9       b.ls #+0x58 (addr 0x170000214)`.
/// Three hex columns then the instruction text; anything shaped differently
/// (constant-pool entries, safepoint rows, SFI context lines) is rejected.
fn parse_disasm_row(text: &str) -> Option<LineInfo> {
    let rest = text.strip_prefix("0x")?;
    let addr_len = hex_run(rest);
    if addr_len == 0 {
        return None;
    }
    let addr = u64::from_str_radix(&text[2..2 + addr_len], 16).ok()?;
    let mut at = 2 + addr_len;

    let spaces = space_run(&text[at..]);
    if spaces == 0 {
        return None;
    }
    at += spaces;
    let off_len = hex_run(&text[at..]);
    if off_len == 0 {
        return None;
    }
    let offset = u32::from_str_radix(&text[at..at + off_len], 16).ok()?;
    let offset_span: Span = at as u32..(at + off_len) as u32;
    // The offset column must be a complete token.
    if !text[at + off_len..].starts_with(' ') {
        return None;
    }
    at += off_len;

    let spaces = space_run(&text[at..]);
    if spaces == 0 {
        return None;
    }
    at += spaces;
    let enc_len = hex_run(&text[at..]);
    if enc_len < 2 || !text[at + enc_len..].starts_with(' ') {
        return None;
    }
    at += enc_len;

    let spaces = space_run(&text[at..]);
    if spaces == 0 {
        return None;
    }
    at += spaces;
    if at >= text.len() {
        return None;
    }

    // The mnemonic token (x64's `REX.W` prefix included, as emitted).
    let mnemonic_len = text[at..]
        .bytes()
        .take_while(|b| !b.is_ascii_whitespace())
        .count();
    let mnemonic: Span = at as u32..(at + mnemonic_len) as u32;

    // Reloc comment: `;; …` to end of line.
    let comment = text[at..]
        .find(";;")
        .map(|c| (at + c) as u32..text.trim_end().len() as u32);

    // Branch target: arm64 `(addr 0x…)`, x64 ` <+0x…>` — searched only up to
    // the comment, which can itself contain an `(addr 0x…)` that is not this
    // row's target (found in review).
    let operand_end = comment
        .as_ref()
        .map(|c| c.start as usize)
        .unwrap_or(text.len());
    let operands = &text[..operand_end];
    let target =
        find_token(operands, at, "(addr 0x", ")").or_else(|| find_token(operands, at, "<+0x", ">"));

    // Resolve the target token to a code offset (`<+0x71>`) or an absolute
    // address (`(addr 0x170000214)`) — the two forms V8's disassemblers
    // print; either finds the destination row.
    let (mut target_offset, mut target_addr) = (None, None);
    if let Some(span) = &target {
        let token = &text[span.start as usize..span.end as usize];
        if let Some(hex) = token.strip_prefix("<+0x").and_then(|t| t.strip_suffix('>')) {
            target_offset = u32::from_str_radix(hex, 16).ok();
        } else if let Some(hex) = token
            .strip_prefix("(addr 0x")
            .and_then(|t| t.strip_suffix(')'))
        {
            target_addr = u64::from_str_radix(hex, 16).ok();
        }
    }

    Some(LineInfo::Disasm {
        offset,
        offset_span,
        addr,
        mnemonic,
        target,
        target_offset,
        target_addr,
        labeled: false,
        comment,
    })
}

/// The span of `open…close` starting at or after `from`, close included.
fn find_token(text: &str, from: usize, open: &str, close: &str) -> Option<Span> {
    let start = from + text[from..].find(open)?;
    let end = start + text[start..].find(close)? + close.len();
    Some(start as u32..end as u32)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn row(text: &str) -> Option<LineInfo> {
        parse_disasm_row(text)
    }

    #[test]
    fn arm64_row_with_target_and_comment() {
        let text = "0x170000234    94  5800033b       ldr cp, pc+100 (addr 0x0000000170000298)    ;; object: 0x09b8010049f9 <NativeContext[310]>";
        let Some(LineInfo::Disasm {
            offset,
            mnemonic,
            target,
            comment,
            ..
        }) = row(text)
        else {
            panic!("disasm row expected");
        };
        assert_eq!(offset, 0x94);
        assert_eq!(&text[mnemonic.start as usize..mnemonic.end as usize], "ldr");
        let t = target.unwrap();
        assert_eq!(
            &text[t.start as usize..t.end as usize],
            "(addr 0x0000000170000298)"
        );
        let c = comment.unwrap();
        assert!(text[c.start as usize..c.end as usize].starts_with(";; object"));
    }

    #[test]
    fn x64_row_with_relative_target() {
        let text = "0x1639401d3    13  0f8658000000         jna 0x163940231  <+0x71>";
        let Some(LineInfo::Disasm {
            offset,
            mnemonic,
            target,
            comment,
            ..
        }) = row(text)
        else {
            panic!("disasm row expected");
        };
        assert_eq!(offset, 0x13);
        assert_eq!(&text[mnemonic.start as usize..mnemonic.end as usize], "jna");
        assert_eq!(
            &text[target.clone().unwrap().start as usize..target.unwrap().end as usize],
            "<+0x71>"
        );
        assert!(comment.is_none());
    }

    #[test]
    fn targets_resolve_and_label_their_destinations() {
        // x64 relative form → code offset; arm64 absolute form → address.
        let Some(LineInfo::Disasm {
            target_offset,
            target_addr,
            ..
        }) = row("0x1639401d3    13  0f8658000000         jna 0x163940231  <+0x71>")
        else {
            panic!("row");
        };
        assert_eq!(target_offset, Some(0x71));
        assert_eq!(target_addr, None);

        let Some(LineInfo::Disasm {
            target_offset,
            target_addr,
            ..
        }) = row("0x1700001bc    1c  540002c9       b.ls #+0x58 (addr 0x170000214)")
        else {
            panic!("row");
        };
        assert_eq!(target_offset, None);
        assert_eq!(target_addr, Some(0x170000214));

        // The label pass marks destinations, by offset and by address.
        let mut buffer = LogBuffer::new();
        buffer.append(
            b"Instructions (size = 40)\n\
0x100    0  aa  jmp 0x108  <+0x8>\n\
0x104    4  bb  b.ls #+0x8 (addr 0x10c)\n\
0x108    8  cc  nop\n\
0x10c    c  dd  ret\n",
        );
        buffer.finish();
        let phase = parse_listing_phase(&buffer, 0..buffer.line_count(), "Instructions");
        let labeled: Vec<bool> = phase
            .infos
            .iter()
            .filter_map(|i| match i {
                LineInfo::Disasm { labeled, .. } => Some(*labeled),
                _ => None,
            })
            .collect();
        assert_eq!(labeled, [false, false, true, true], "+0x8 and 0x10c");
    }

    #[test]
    fn lookalikes_are_rejected() {
        // SFI context line: `<` after the address, not a hex column.
        assert!(row("0x09b80101e2cd <SharedFunctionInfo add> (…)").is_none());
        // Constant-pool entry: colon after the address.
        assert!(row("0x31a01000085: [TrustedFixedArray]").is_none());
        // Safepoint row: third column is not hex.
        assert!(row("0x17000023c     9c  slots (sp->fp): 100000").is_none());
        // RelocInfo row: second column is not hex.
        assert!(row("0x170000234  full embedded object  (0x09b8…)").is_none());
        // Deopt-data table row: no 0x prefix.
        assert!(row("     0                2    NA ").is_none());
    }

    #[test]
    fn x64_variable_length_encodings_parse() {
        let text = "0x1639401c1     1  488bec               REX.W movq rbp,rsp";
        let Some(LineInfo::Disasm {
            offset, mnemonic, ..
        }) = row(text)
        else {
            panic!()
        };
        assert_eq!(offset, 1);
        assert_eq!(
            &text[mnemonic.start as usize..mnemonic.end as usize],
            "REX.W"
        );
    }
}
