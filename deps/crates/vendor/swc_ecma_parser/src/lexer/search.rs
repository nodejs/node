//! Utilities inspired by OXC lexer for fast byte-wise searching over source
//! text.

/// How many bytes we process per batch when scanning.
pub const SEARCH_BATCH_SIZE: usize = 32;

/// Compile-time lookup table guaranteeing UTF-8 boundary safety.
#[repr(C, align(64))]
pub struct SafeByteMatchTable([bool; 256]);

impl SafeByteMatchTable {
    pub const fn new(bytes: [bool; 256]) -> Self {
        // Safety guarantee: either all leading bytes (0xC0..0xF7) match, or all
        // continuation bytes (0x80..0xBF) *do not* match. This ensures that if
        // we stop on a match the input cursor is on a UTF-8 char boundary.
        let mut unicode_start_all_match = true;
        let mut unicode_cont_all_no_match = true;
        let mut i = 0;
        while i < 256 {
            let m = bytes[i];
            if m {
                if i >= 0x80 && i < 0xc0 {
                    unicode_cont_all_no_match = false;
                }
            } else if i >= 0xc0 && i < 0xf8 {
                unicode_start_all_match = false;
            }
            i += 1;
        }
        assert!(
            unicode_start_all_match || unicode_cont_all_no_match,
            "Cannot create SafeByteMatchTable with an unsafe pattern"
        );
        Self(bytes)
    }

    #[inline]
    pub const fn use_table(&self) {}

    #[inline]
    pub const fn matches(&self, b: u8) -> bool {
        self.0[b as usize]
    }
}

// ------------------------- Macros -------------------------

#[macro_export]
macro_rules! safe_byte_match_table {
    (|$byte:ident| $body:expr) => {{
        use $crate::lexer::search::SafeByteMatchTable;
        #[allow(clippy::eq_op, clippy::allow_attributes)]
        const TABLE: SafeByteMatchTable = seq_macro::seq!($byte in 0u8..=255 {
            SafeByteMatchTable::new([#($body,)*])
        });
        TABLE
    }};
}

#[macro_export]
/// Macro to search for first byte matching a `ByteMatchTable` or
/// `SafeByteMatchTable`.
///
/// Search processes source in batches of `SEARCH_BATCH_SIZE` bytes for speed.
/// When not enough bytes remaining in source for a batch, search source byte by
/// byte.
///
/// The search process is pointer-based and bumps the lexer at the end. So pay
/// attention not to change the pos of lexer in `continue_if`.
macro_rules! byte_search {
    // Simple version without continue_if
    (
        lexer: $lexer:ident,
        table: $table:ident,
        handle_eof: $eof_handler:expr $(,)?
    ) => {
        byte_search! {
            lexer: $lexer,
            table: $table,
            continue_if: (_byte, _pos) false,
            handle_eof: $eof_handler,
        }
    };

    // Full version with continue_if support
    (
        lexer: $lexer:ident,
        table: $table:ident,
        continue_if: ($byte:ident, $pos:ident) $should_continue:expr,
        handle_eof: $eof_handler:expr $(,)?
    ) => {{
        $table.use_table();
        let mut $pos = 0;
        let bytes = $lexer.input().as_str().as_bytes();
        let len = bytes.len();
        let bytes = bytes.as_ptr();

        let $byte = 'outer: loop {
            let batch_end = $pos + $crate::lexer::search::SEARCH_BATCH_SIZE;
            let $byte = if batch_end < len {
                // Safety: `batch_end < len`
                let batch = unsafe {
                    std::slice::from_raw_parts(
                        bytes.add($pos),
                        $crate::lexer::search::SEARCH_BATCH_SIZE,
                    )
                };
                'inner: loop {
                    for (i, &byte) in batch.iter().enumerate() {
                        if $table.matches(byte) {
                            // We find a matched byte, jump out to check with continue_if
                            $pos += i;
                            break 'inner byte;
                        }
                    }

                    // We don't find a matched byte in this batch,
                    // So continue to try the next batch/remaining
                    $pos = batch_end;
                    continue 'outer;
                }
            } else {
                'inner: loop {
                    // The remaining is shorter than batch size.
                    let remaining =
                        unsafe { std::slice::from_raw_parts(bytes.add($pos), len - $pos) };
                    for (i, &byte) in remaining.iter().enumerate() {
                        if $table.matches(byte) {
                            // We find a matched byte, jump out to check with continue_if
                            $pos += i;
                            break 'inner byte;
                        }
                    }

                    // We don't find a matched byte in the remaining,
                    // which also means we have reached the end of the input.
                    unsafe {
                        $lexer.input_mut().bump_bytes(len);
                    }
                    $eof_handler
                }
            };

            // Check if we should continue searching
            if $should_continue {
                // Continue searching from next position
                $pos += 1;
                continue;
            }

            break $byte;
        };

        unsafe {
            $lexer.input_mut().bump_bytes($pos);
        }
        $byte
    }};
}
