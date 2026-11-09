#!/usr/bin/env bash
# Luma3DS v9.1 / v13.4 デュアルビルド
#
# v9.1 → v13.4 の順に続けてビルドする。前段が失敗したら次には進まない。
#
#   ./build.sh                   両方ビルド
#   ONLY=v9.1 ./build.sh         v9.1 だけ
#   ONLY=v13.4 ./build.sh        v13.4 だけ（v9.1 の成果物は残っていれば dist に入る）
#   KEEP_BUILD=1 ./build.sh      make clean を省いて差分ビルド
#   STRICT=1 ./build.sh          全警告をエラーにして棚卸し
#   K11_MAX=0x10000 ./build.sh   k11_extension が指定サイズを超えたら失敗させる
#
set -Eeuo pipefail

: "${DEVKITPRO:?export DEVKITPRO first}"
: "${DEVKITARM:?export DEVKITARM first}"

ROOT="$(cd "$(dirname "$0")" && pwd)"

# devkitARM の 3ds_rules も libctru の Makefile も $(CURDIR) をクォートせずに
# 再帰 make へ渡す。パスに空白や括弧が入っていると、その先の bash が
# 「予期しないトークン `(' 周辺に構文エラー」で落ちる。
# ここで弾かないと libctru のビルド中に意味不明なエラーとして出る。
case "$ROOT" in
  *[!A-Za-z0-9/._-]*)
    echo "!! このフォルダのパスに、make が扱えない文字が含まれています:" >&2
    echo "     $ROOT" >&2
    echo >&2
    echo "   devkitARM の Makefile はパスをクォートしないため、空白・括弧・" >&2
    echo "   日本語などが入っているとビルドできません。" >&2
    echo "   英数字と _ - . だけのパスに移動するか、フォルダ名を変えてください。" >&2
    echo "   例: Luma3DS_Source_patched (1)  ->  Luma3DS_Source_patched" >&2
    exit 1 ;;
esac

CACHE="$ROOT/.libctru"
DIST="$ROOT/dist"
LOG="$ROOT/build.log"
: > "$LOG"

V91_DIR="$ROOT/Luma3DS_v9_1"
V134_DIR="$ROOT/Luma3DS_v13_4"
# v13.4 は qtmGetSessionHandle() を使う。これは libctru v2.4.0 で追加された。
# 上流は「libctru と Luma3DS のリリースは同期している」としているので、
# v13.4 (2026-04) の直前のリリースである v2.7.0 を既定にする。
# 相性が悪ければ V134_CTRU=v2.6.2 ./build.sh のように上書きできる。
V91_CTRU="${V91_CTRU:-v2.2.2}"
V134_CTRU="${V134_CTRU:-v2.7.0}"

NM="$DEVKITARM/bin/arm-none-eabi-nm"

ONLY="${ONLY:-}"
case "$ONLY" in
  ""|v9.1|v13.4) ;;
  *) echo "ONLY は v9.1 か v13.4 を指定してください（指定値: $ONLY）" >&2; exit 1 ;;
esac

STAGE="起動処理"
trap 'echo; echo "!! 失敗: $STAGE"; echo "   詳細は build.log を確認してください。"; echo "   v13.4 だけやり直すなら: ONLY=v13.4 ./build.sh"' ERR

stage() {
  STAGE="$1"
  echo
  echo "==================================================================="
  echo "  $1"
  echo "==================================================================="
}

# make が 0 を返しても成果物が無いことがあるので、実体を必ず確認する。
verify_artifact() {
  local f="$1" label="$2"
  if [ ! -s "$f" ]; then
    echo "  !! $label: boot.firm が生成されていません ($f)" >&2
    return 1
  fi
  printf '  %-8s boot.firm OK (%s bytes)\n' "$label" "$(stat -c %s "$f")"
}

# --- firmtool の場所を自動で足す -------------------------------------------
# pip でインストールした firmtool は Windows 側の Scripts に入るため、
# devkitPro の MSYS2 からは既定で見えない。よくある場所を PATH に足しておく。
if ! command -v firmtool >/dev/null 2>&1; then
  for d in \
      "$HOME/.local/bin" \
      /c/Users/*/AppData/Local/Programs/Python/Python3*/Scripts \
      /c/Users/*/AppData/Roaming/Python/Python3*/Scripts \
      /c/Python3*/Scripts \
      /mingw64/bin
  do
    [ -x "$d/firmtool" ] || [ -x "$d/firmtool.exe" ] || continue
    PATH="$PATH:$d"
    echo "firmtool を追加: $d"
    break
  done
fi
export PATH

# 存在確認だけでなく実際に起動する。「PATH にはあるがランタイム DLL が無くて
# 起動できない」ケースは command -v では素通りし、make の中で 127 になって
# 原因が見えなくなるため。
#
# 終了コードだけでは「起動できなかった 127」と「起動したがその引数を
# 受け付けず 127 を返した」を区別できない。出力が 1 バイトでもあれば
# プロセスは動いているので、無出力かつ 126 以上のときだけ失敗とみなす。
need() {
  local tool="$1" probe="${2:-}" path out rc=0
  path="$(command -v "$tool" 2>/dev/null || true)"
  if [ -z "$path" ]; then
    echo "!! $tool が PATH にありません。" >&2
    return 1
  fi
  out="$("$tool" $probe 2>&1)" || rc=$?
  if [ "$rc" -ge 126 ] && [ -z "$out" ]; then
    echo "!! $tool は $path にありますが起動できません (終了コード $rc、出力なし)。" >&2
    echo "   Windows ネイティブの exe なら必要なランタイム DLL が足りていません。" >&2
    echo "   32bit なら vc_redist.x86、64bit なら vc_redist.x64 を入れてください。" >&2
    echo "   ビット数の確認: file $path" >&2
    return 1
  fi
  printf '  %-10s %s\n' "$tool" "$path"
}

echo "使用するツール:"
toolfail=0
need git --version       || toolfail=1
need make --version      || toolfail=1
need makerom             || toolfail=1
need firmtool --help     || toolfail=1
if [ "$ONLY" != "v13.4" ]; then
  need armips --version  || toolfail=1
fi
if [ "$toolfail" != "0" ]; then
  echo >&2
  echo "必要なツールが揃っていません。" >&2
  echo "armips が使えないだけなら、v13.4 は armips を必要としないので回避できます:" >&2
  echo "  ONLY=v13.4 ./build.sh    (Windows: set ONLY=v13.4 & build.bat)" >&2
  echo "makerom / firmtool は両バージョンで必要なので、これらは回避できません。" >&2
  exit 1
fi
echo

build_libctru() {
  local ver="$1"
  local prefix="$CACHE/$ver"
  [ -f "$prefix/lib/libctru.a" ] && return 0
  local src="$CACHE/src-$ver"
  rm -rf "$src"
  git clone --quiet --depth 1 --branch "$ver" https://github.com/devkitPro/libctru "$src"
  # libctru 自身が -Werror を付けているため、新しい GCC では古い libctru が通らない。
  # ここは依存ライブラリなので警告は警告のまま流す（Luma 本体の HARDERRORS には影響しない）。
  sed -i 's/-Werror//g' "$src/libctru/Makefile"
  make -C "$src/libctru" -j"$(nproc)" 2>&1 | tee -a "$LOG"
  rm -rf "$prefix"
  mkdir -p "$prefix"
  cp -a "$src/libctru/include" "$prefix/include"
  cp -a "$src/libctru/lib" "$prefix/lib"
  rm -rf "$src"
}

build_tree() {
  local dir="$1" ver="$2" target="$3"
  [ -d "$dir" ] || { echo "not found: $dir" >&2; exit 1; }
  if [ "${KEEP_BUILD:-0}" != "1" ]; then
    make -C "$dir" clean >/dev/null 2>&1 || true
  fi
  CTRULIB="$CACHE/$ver" STRICT="${STRICT:-0}" make -C "$dir" "$target" 2>&1 | tee -a "$LOG"
}

# k11_extension のサイズ = __end__ - ベースVA。
# arm9/source/patches.c は Old 3DS でこのサイズぶんを 0x26C00000 の下、
# BASE メモリ領域から削り取る。ここが膨らむと O3DS だけ起動に失敗しうる。
report_kext() {
  local elf="$1" base="$2" label="$3"
  [ -f "$elf" ] || { echo "  $label: (elf が見つかりません: $elf)"; return 0; }
  local end
  end="$("$NM" "$elf" | awk '$3 == "__end__" { print $1; exit }')"
  if [ -z "$end" ]; then
    echo "  $label: __end__ を取得できませんでした"
    return 0
  fi
  local size=$(( 0x$end - base ))
  printf '  %-8s k11_extension size = %d bytes (0x%X)\n' "$label" "$size" "$size"
  if [ -n "${K11_MAX:-}" ] && [ "$size" -gt "$((K11_MAX))" ]; then
    echo "  !! $label: k11_extension が K11_MAX ($((K11_MAX)) bytes) を超えています" >&2
    exit 1
  fi
}

V91_FIRM="$V91_DIR/out/boot.firm"
V134_FIRM="$V134_DIR/boot.firm"

do_v91=1;  [ "$ONLY" = "v13.4" ] && do_v91=0
do_v134=1; [ "$ONLY" = "v9.1"  ] && do_v134=0

# --- 1段目: v9.1 -----------------------------------------------------------
if [ "$do_v91" = "1" ]; then
  stage "[1/2] Luma3DS v9.1 をビルド中 (libctru $V91_CTRU)"
  build_libctru "$V91_CTRU"
  build_tree "$V91_DIR" "$V91_CTRU" firm
  verify_artifact "$V91_FIRM" "v9.1"
  report_kext "$V91_DIR/k11_extension/build/k11_extension.elf" 0x40000000 "v9.1"
  if [ "$do_v134" = "1" ]; then
    echo
    echo "  v9.1 のビルドに成功しました。続けて v13.4 をビルドします。"
  fi
fi

# --- 2段目: v13.4（1段目が成功した場合のみここに到達する） -----------------
if [ "$do_v134" = "1" ]; then
  stage "[2/2] Luma3DS v13.4 をビルド中 (libctru $V134_CTRU)"
  build_libctru "$V134_CTRU"
  build_tree "$V134_DIR" "$V134_CTRU" all
  verify_artifact "$V134_FIRM" "v13.4"
  report_kext "$V134_DIR/k11_extension/k11_extension.elf" 0x70000000 "v13.4"
fi

# --- 配置 ------------------------------------------------------------------
stage "SD カード用に dist/ を作成中"
rm -rf "$DIST"
mkdir -p "$DIST/3ds/luma/9.1" "$DIST/3ds/luma/13.4" "$DIST/luma"

copied_default=0
if [ -s "$V134_FIRM" ]; then
  cp "$V134_FIRM" "$DIST/3ds/luma/13.4/boot.firm"
  cp "$V134_FIRM" "$DIST/boot.firm"   # 既定で起動する版
  copied_default=1
else
  echo "  v13.4 の boot.firm が無いので dist に入れません。"
fi

if [ -s "$V91_FIRM" ]; then
  cp "$V91_FIRM" "$DIST/3ds/luma/9.1/boot.firm"
  # v13.4 が無ければ v9.1 を既定にする
  [ "$copied_default" = "1" ] || cp "$V91_FIRM" "$DIST/boot.firm"
else
  echo "  v9.1 の boot.firm が無いので dist に入れません。"
fi

echo
echo "=== 要確認の警告（-fpermissive が隠していた型の問題） ==="
if grep -hoE "\[-W(int-conversion|incompatible-pointer-types|implicit-function-declaration|implicit-int|return-mismatch|return-type)[^]]*\]" "$LOG" | sort | uniq -c | sed 's/^/  /' | grep .; then
  echo "  (詳細は build.log を参照)"
else
  echo "  なし"
fi


echo
echo "=== 成果物 ==="
for f in "$V91_FIRM" "$V134_FIRM"; do
  [ -s "$f" ] && ls -l "$f"
done
echo
echo "dist/ を SD カードのルートにコピーしてください:"
find "$DIST" -type f | sed "s|$DIST|  SD:|"

trap - ERR
echo
echo "完了しました。"
