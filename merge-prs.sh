#!/bin/bash

# GitHub PR Merge Script
# Merges security updates and dependency fixes
# Usage: bash merge-prs.sh

set -e

echo "Starting PR merge process..."
echo "================================"

# SECURITY & DEPENDENCY UPDATE PRs - MERGE THESE
echo "Merging security and dependency update PRs..."

# Node repo
echo "Merging piyyy314/node#40..."
gh pr merge piyyy314/node/40 --merge || echo "⚠️  Failed to merge node#40"

echo "Merging piyyy314/node#41..."
gh pr merge piyyy314/node/41 --merge || echo "⚠️  Failed to merge node#41"

# Kilocode
echo "Merging piyyy314/kilocode#2..."
gh pr merge piyyy314/kilocode/2 --merge || echo "⚠️  Failed to merge kilocode#2"

# AWS Toolkit VSCode
echo "Merging piyyy314/aws-toolkit-vscode#33..."
gh pr merge piyyy314/aws-toolkit-vscode/33 --merge || echo "⚠️  Failed to merge aws-toolkit-vscode#33"

echo "Merging piyyy314/aws-toolkit-vscode#30..."
gh pr merge piyyy314/aws-toolkit-vscode/30 --merge || echo "⚠️  Failed to merge aws-toolkit-vscode#30"

# ADK-JS
echo "Merging piyyy314/adk-js#79..."
gh pr merge piyyy314/adk-js/79 --merge || echo "⚠️  Failed to merge adk-js#79"

# Telegram Apps
echo "Merging piyyy314/telegram-apps#1..."
gh pr merge piyyy314/telegram-apps/1 --merge || echo "⚠️  Failed to merge telegram-apps#1"

# Node.js PubSub
echo "Merging piyyy314/nodejs-pubsub#1..."
gh pr merge piyyy314/nodejs-pubsub/1 --merge || echo "⚠️  Failed to merge nodejs-pubsub#1"

echo "Merging piyyy314/nodejs-pubsub#2..."
gh pr merge piyyy314/nodejs-pubsub/2 --merge || echo "⚠️  Failed to merge nodejs-pubsub#2"

# Highload Wallet Contract
echo "Merging piyyy314/highload-wallet-contract-v3#1..."
gh pr merge piyyy314/highload-wallet-contract-v3/1 --merge || echo "⚠️  Failed to merge highload-wallet-contract-v3#1"

# Midscene
echo "Merging piyyy314/midscene#3..."
gh pr merge piyyy314/midscene/3 --merge || echo "⚠️  Failed to merge midscene#3"

echo "Merging piyyy314/midscene#4..."
gh pr merge piyyy314/midscene/4 --merge || echo "⚠️  Failed to merge midscene#4"

echo "Merging piyyy314/midscene#5..."
gh pr merge piyyy314/midscene/5 --merge || echo "⚠️  Failed to merge midscene#5"

# Personal AI Agent
echo "Merging piyyy314/personal-ai-agent#48..."
gh pr merge piyyy314/personal-ai-agent/48 --merge || echo "⚠️  Failed to merge personal-ai-agent#48"

echo ""
echo "================================"
echo "✅ Security and dependency update merges complete!"
echo ""
echo "Note: Feature/UX improvement PRs were NOT merged:"
echo "  - Multiple 'Palette' CLI UX improvements in adk-js"
echo "  - Documentation PRs in Shiatoali-"
echo "  - Template PRs in external repos"
echo ""
echo "Review these manually if you'd like to merge them."
