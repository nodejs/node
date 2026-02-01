#!/bin/bash

# --- SKRIP OTOMATIS PULL REQUEST ---

echo "========================================"
echo "🤖 GITHUB AUTO PULL REQUEST BOT"
echo "========================================"

# 1. Cek apakah ada file yang berubah
if [[ -z $(git status -s) ]]; then
  echo "❌ ERROR: Kamu belum mengedit file apapun!"
  echo "   TIPS: Edit dulu file README.md pakai 'nano README.md'"
  echo "         Lalu jalankan script ini lagi."
  exit 1
fi

# 2. Input Data dari User
read -p "📂 Nama Branch Baru (cth: fix-typo-readme): " BRANCH_NAME
read -p "📝 Pesan Commit (cth: fix: add 'more' to readme): " COMMIT_MSG
read -p "heading Judul PR (cth: Docs: Add missing word): " PR_TITLE
read -p "📄 Deskripsi PR (cth: Added 'more' for better readability): " PR_BODY

echo "----------------------------------------"
echo "⏳ Memproses..."

# 3. Membuat Branch Baru
echo "[1/4] Membuat branch '$BRANCH_NAME'..."
git checkout -b "$BRANCH_NAME"

# 4. Simpan Perubahan (Add & Commit)
echo "[2/4] Menyimpan perubahan..."
git add .
git commit -m "$COMMIT_MSG"

# 5. Upload ke GitHub (Push)
echo "[3/4] Mengupload ke GitHub..."
git push --set-upstream origin "$BRANCH_NAME"

# 6. Membuat Pull Request via GitHub CLI
echo "[4/4] Membuat Pull Request..."
gh pr create --title "$PR_TITLE" --body "$PR_BODY" --base main

# 7. Cek Hasil
if [ $? -eq 0 ]; then
    echo "----------------------------------------"
    echo "✅ SUKSES! Pull Request berhasil dikirim."
    echo "🔗 Cek link di atas untuk melihat PR kamu."
else
    echo "----------------------------------------"
    echo "❌ GAGAL. Cek pesan error di atas."
fi

