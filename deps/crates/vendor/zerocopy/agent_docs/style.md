<!-- Copyright 2025 The Fuchsia Authors

Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
<LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
This file may not be copied, modified, or distributed except according to
those terms. -->

# Style Guidelines

This document covers code style and formatting guidelines for the project, as
well as commit message requirements.

## File Headers

Each file must contain a copyright header (see `src/lib.rs` for example) which is
based on that file's creation year.

## Formatting

Refer to `ci/check_fmt.sh`.

## Comments

- Wrap all comments (`//`, `///`, `//!`) at **80 columns** from the left margin,
  taking into account any preceding code or comments.
- **Exceptions:** Markdown tables, ASCII diagrams, long URLs, code blocks, or
  other cases where wrapping would impair readability.

## Markdown Files

- Wrap paragraphs and bulleted lists at **80 columns** from the left margin,
  taking into account any preceding code or comments. For example, a markdown
  block inside of a `/// Lorem ipsum...` comment should have lines no more than
  76 columns wide.
  - In bulleted lists, indent subsequent lines by 2 spaces.
  - Do not wrap links if it breaks them.
- Always put a blank line between a section header and the beginning of the section.

## Pull Requests and Commit Messages

Use GitHub issue syntax in commit messages:

- Resolves issue: `Closes #123`
- Progress on issue: `Makes progress on #123`
