#!/bin/bash
# Luma3DS_new を上流（LumaTeam/Luma3DS）の新しい版に上げる。
#
#   bash tools/upgrade_upstream.sh v13.6       上げ始める
#   bash tools/upgrade_upstream.sh --finish    ぶつかったところを直したあとで、仕上げる
#   bash tools/upgrade_upstream.sh --abort     やめる（Luma3DS_new は変えない）
#
# やり方: 上流の今の版（Luma3DS_new/UPSTREAM_VERSION）の上に Luma3DS_new の中身を 1 つのコミットとして載せ、
# それを新しい版の上に git cherry-pick で載せ直す（3-way マージ）。改変版と上流が同じところを変えていれば、
# .upgrade/luma の中にぶつかったところ（<<<<<<< と >>>>>>>）が残るので、直して git add し、--finish を実行する。
# 仕上げると Luma3DS_new を新しい中身にし、UPSTREAM_VERSION を書き換える。
# そのあとの作業は tools/README.md を参照（新しい英語の文字列の日本語化、字形の作り直し、ビルドの確認など）。

set -Eeuo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
NEW_DIR="$ROOT/Luma3DS_new"
WORK="$ROOT/.upgrade"
REPO="$WORK/luma"
UPSTREAM_URL="${UPSTREAM_URL:-https://github.com/LumaTeam/Luma3DS}"

git_q() { git -C "$REPO" -c user.name=upgrade -c user.email=upgrade@localhost -c core.autocrlf=false "$@"; }

finish() {
  if [ ! -d "$REPO/.git" ]; then
    echo "!! 上げている途中の作業がありません。" >&2
    exit 1
  fi
  if git_q diff --name-only --diff-filter=U | grep -q .; then
    echo "!! まだぶつかったままのファイルがあります:" >&2
    git_q diff --name-only --diff-filter=U >&2
    exit 1
  fi
  if grep -rIl --exclude-dir=.git '^<<<<<<< \|^>>>>>>> ' "$REPO" >/dev/null; then
    echo "!! <<<<<<< か >>>>>>> が残っているファイルがあります:" >&2
    grep -rIl --exclude-dir=.git '^<<<<<<< \|^>>>>>>> ' "$REPO" >&2
    exit 1
  fi
  git_q add -A
  if [ -d "$REPO/.git/sequencer" ] || [ -f "$REPO/.git/CHERRY_PICK_HEAD" ]; then
    GIT_EDITOR=true git_q cherry-pick --continue >/dev/null
  fi

  local tag
  tag="$(cat "$WORK/target")"
  rm -rf "$NEW_DIR.old"
  mv "$NEW_DIR" "$NEW_DIR.old"
  mkdir "$NEW_DIR"
  git_q archive HEAD | tar -x -C "$NEW_DIR"
  printf '%s\n' "$tag" > "$NEW_DIR/UPSTREAM_VERSION"
  rm -rf "$NEW_DIR.old" "$WORK"
  echo "Luma3DS_new を $tag にしました。tools/README.md の「上げたあとにすること」を進めてください。"
}

case "${1:-}" in
  --finish) finish; exit 0 ;;
  --abort)  rm -rf "$WORK"; echo "やめました。Luma3DS_new は変えていません。"; exit 0 ;;
  v[0-9]*)  TARGET="$1" ;;
  *) echo "使い方: bash tools/upgrade_upstream.sh <上流の版（例: v13.6）> | --finish | --abort" >&2; exit 1 ;;
esac

BASE="$(tr -d ' \r\n' < "$NEW_DIR/UPSTREAM_VERSION")"
if [ -d "$WORK" ]; then
  echo "!! 前の作業（.upgrade）が残っています。--finish か --abort を先に実行してください。" >&2
  exit 1
fi
if [ "$BASE" = "$TARGET" ]; then
  echo "Luma3DS_new はもう $TARGET です。"
  exit 0
fi

mkdir -p "$WORK"
printf '%s\n' "$TARGET" > "$WORK/target"
echo "上流の $BASE と $TARGET を取ってきています..."
git clone --quiet --no-checkout "$UPSTREAM_URL" "$REPO"
git_q checkout --quiet -b fork "$BASE"

# 上流の今の版の上に、改変版の中身を 1 つのコミットとして載せる
git_q rm -rq --cached .
find "$REPO" -mindepth 1 -maxdepth 1 ! -name .git -exec rm -rf {} +
(cd "$NEW_DIR" && tar -cf - --exclude=./UPSTREAM_VERSION .) | tar -x -C "$REPO"
# 改変版のビルドで出来たものは載せない
(cd "$REPO" && make -s clean >/dev/null 2>&1 || true)
git_q add -A
git_q commit --quiet -m "boot.firm_Customize-Model fork on $BASE"

# 新しい版の上に載せ直す
git_q checkout --quiet -b upgrade "$TARGET"
if git_q cherry-pick fork >/dev/null 2>&1; then
  echo "ぶつかるところはありませんでした。"
  finish
else
  echo
  echo "改変版と上流の両方が変えたところがあります。次のファイルの <<<<<<< と >>>>>>> の間を直してください"
  echo "（上が上流の $TARGET、下が改変版）:"
  git_q diff --name-only --diff-filter=U | sed 's/^/  .upgrade\/luma\//'
  echo
  echo "直したら: bash tools/upgrade_upstream.sh --finish"
  echo "やめるなら: bash tools/upgrade_upstream.sh --abort"
  exit 2
fi
