# Automatic Upstream Sync Setup

This repository has been configured to automatically sync with the upstream Node.js repository (`nodejs/node`).

## How it works

### Automatic Sync (GitHub Actions)

The repository includes a GitHub Actions workflow (`.github/workflows/sync-upstream.yml`) that:

- **Runs daily** at 6:00 AM UTC
- **Can be triggered manually** via GitHub Actions UI
- **Fetches latest changes** from the upstream Node.js repository
- **Attempts fast-forward merge** if possible
- **Creates a pull request** if conflicts occur that need manual resolution

### Manual Sync (Shell Script)

For manual synchronization, you can use the `sync-upstream.sh` script:

```bash
./sync-upstream.sh
```

This script will:
- Add the upstream remote if needed
- Fetch latest changes
- Attempt to fast-forward merge
- Provide guidance if conflicts occur

## Sync Behavior

### Successful Automatic Sync
When the upstream has new commits and they can be cleanly fast-forwarded:
1. The workflow creates/updates the `main` branch
2. Merges upstream changes via fast-forward
3. Pushes the updated `main` branch to origin
4. ✅ Sync complete!

### Conflict Resolution Required
When automatic fast-forward fails (due to conflicts or divergent history):
1. The workflow creates a pull request with title: "sync: Update from upstream Node.js repository"
2. The PR includes details about the upstream commit and conflicts
3. 👤 Manual review and merge required

## Repository Structure

- **origin**: `https://github.com/dharani-dharan-11023/node` (this fork)
- **upstream**: `https://github.com/nodejs/node` (original Node.js repository)

## Manual Operations

### Check sync status
```bash
git fetch upstream
git log --oneline HEAD..upstream/main  # See what's new upstream
```

### Manual sync with conflicts
If automatic sync creates a PR due to conflicts:
```bash
git checkout main
git fetch upstream
git merge upstream/main  # or git rebase upstream/main
# Resolve conflicts if any
git push origin main
```

### Force sync (discard local changes)
⚠️ **Warning**: This discards any local commits not in upstream
```bash
git checkout main
git fetch upstream
git reset --hard upstream/main
git push origin main --force
```

## Workflow Configuration

The sync workflow can be customized by editing `.github/workflows/sync-upstream.yml`:

- **Schedule**: Modify the `cron` expression to change frequency
- **Repository filter**: Update the `if` condition to match your repository
- **Branch name**: Change the target branch from `main` if needed

## Security Notes

- The workflow uses `GITHUB_TOKEN` for standard operations
- No additional secrets required for basic sync functionality
- Pull requests are created with appropriate labels for easy identification