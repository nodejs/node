#!/usr/bin/env vpython3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from __future__ import annotations
import json
import sys
from pathlib import Path


def analyze_brain(brain_id: str) -> None:
  brain_dir = Path.home() / f".gemini/jetski/brain/{brain_id}"
  if not brain_dir.exists():
    print(f"Brain directory {brain_dir} not found.")
    return

  message_files = list(brain_dir.glob(".system_generated/messages/*.json"))
  if not message_files:
    print("No messages found in brain.")
    return

  print(
      f"Analyzing {len(message_files)} messages for potential shortcuts or flaws..."
  )

  keywords = [
      "shortcut", "skip", "lazy", "ignore", "failed", "error", "timeout",
      "hardcoded", "workaround"
  ]

  findings = []
  for msg_file in sorted(message_files):
    try:
      with msg_file.open("r") as f:
        msg = json.load(f)
        content = msg.get("content", "").lower()
        for kw in keywords:
          if kw in content:
            findings.append(
                f"- Found '{kw}' in message {msg['id']} from {msg['sender']}: {msg['content'][:200]}..."
            )
            break
    except Exception as e:
      print(f"Error analyzing message {msg_file}: {e}")
      pass

  if findings:
    print("\n Potential shortcuts or issues detected:")
    for finding in findings:
      print(finding)
  else:
    print("\nNo obvious shortcuts or issues detected via keywords.")


if __name__ == "__main__":
  if len(sys.argv) < 2:
    print("Usage: analyze_brain.py <brain_id>")
    sys.exit(1)
  analyze_brain(sys.argv[1])
