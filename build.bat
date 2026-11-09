:<<"::BASH_BEGIN"
@echo off
setlocal EnableExtensions
title Luma3DS v13.4 / v9.1 build
cd /d "%~dp0"

set "RC=0"
set "BASH="
if defined DEVKITPRO if exist "%DEVKITPRO%\msys2\usr\bin\bash.exe" set "BASH=%DEVKITPRO%\msys2\usr\bin\bash.exe"
if not defined BASH if exist "C:\devkitPro\msys2\usr\bin\bash.exe" set "BASH=C:\devkitPro\msys2\usr\bin\bash.exe"
if defined BASH goto :run
echo [ERROR] devkitPro no bash.exe ga mitsukarimasen.
echo   devkitPro wo irete kudasai: https://devkitpro.org/wiki/Getting_Started
echo   C:\devkitPro igai ni ireta baai wa, saki ni  set DEVKITPRO=D:\devkitPro  to shite kudasai.
set "RC=1"
goto :done

:run
rem CHERE_INVOKING=1 stops the MSYS2 login shell from cd-ing to $HOME,
rem so the bash part runs from this folder. It locates itself anyway.
set "CHERE_INVOKING=1"
set "MSYSTEM=MSYS"
"%BASH%" -l "%~nx0" %1
set "RC=%ERRORLEVEL%"

:done
echo.
if "%~1"=="" pause
exit /b %RC%
::BASH_BEGIN
# Luma3DS v13.4 / v9.1 ビルド
#
# このファイル（build.bat）の後半（bash 部分）を devkitPro の MSYS2 で実行する。
# Makefile が make・sed・awk・git・armips・makerom・firmtool などの Unix 系
# ツールを使うため、cmd.exe からは直接ビルドできない。
#
#   build.bat      メニューから選ぶ（Enter だけなら 1）
#   build.bat 1    v13.4 と v9.1 をビルド
#   build.bat 2    v13.4 だけビルド
#   build.bat 3    v9.1 だけビルド
#   build.bat 4    v13.4 と v9.1 を make clean
#
# Linux などでは bash build.bat [1-4 または all|v13.4|v9.1|clean] で同じことができる。
# ビルドは毎回 make clean してから make する。前の版が失敗したら次には進まない。
#   STRICT=1 bash build.bat 1          全警告をエラーにして棚卸し
#   K11_MAX=0x10000 bash build.bat 1   k11_extension が指定サイズを超えたら失敗させる
#
# 最初に一度だけ必要な準備（Windows）:
#   1) devkitPro を入れて 3DS 開発環境を追加する
#        https://devkitpro.org/wiki/Getting_Started
#        dkp-pacman -S 3ds-dev
#      3ds-dev は 3dslink / 3dstools / devkitARM / devkitARM-gdb /
#      general-tools / picasso / tex3ds の組で、armips は含まれない。
#      armips は v9.1 だけが使うので別に入れる。
#        https://github.com/Kingcom/armips/releases
#      armips.exe を PATH の通ったフォルダ（例: C:\devkitPro\msys2\usr\local\bin）に置く。
#      32bit 版は Visual C++ 2015 x86 再頒布可能パッケージが無いと起動できず、
#      make がエラー 127 を出す。
#   2) pip install git+https://github.com/TuxSH/firmtool.git
#
set -Eeuo pipefail

SEL="${1:-}"
if [ -z "$SEL" ]; then
  echo
  echo "  1: v13.4 と v9.1 をビルド"
  echo "  2: v13.4 だけビルド"
  echo "  3: v9.1 だけビルド"
  echo "  4: v13.4 と v9.1 を make clean"
  echo
  read -r -p "番号を入力して Enter（Enter だけなら 1）: " SEL || true
  echo
fi
case "${SEL:-1}" in
  1|all)   MODE=all ;;
  2|v13.4) MODE=v13.4 ;;
  3|v9.1)  MODE=v9.1 ;;
  4|clean) MODE=clean ;;
  *) echo "!! 1 から 4 の番号を指定してください（指定値: $SEL）" >&2; exit 1 ;;
esac

: "${DEVKITPRO:?DEVKITPRO が設定されていません}"
: "${DEVKITARM:?DEVKITARM が設定されていません}"

ROOT="$(cd "$(dirname "$0")" && pwd)"

# devkitARM の 3ds_rules も libctru の Makefile も $(CURDIR) をクォートせずに
# 再帰 make へ渡す。パスに空白や括弧が入っていると、その先の bash が
# 「予期しないトークン `(' 周辺に構文エラー」で落ちる。
# ここで弾かないと libctru のビルド中に意味不明なエラーとして出る。
case "$ROOT" in
  *[!A-Za-z0-9/._-]*)
    echo "!! このフォルダのパスに、make が扱えない文字が含まれています:" >&2
    echo "     $ROOT" >&2
    echo "   空白・括弧・日本語などを含まない、英数字と _ - . だけのパスに移してください。" >&2
    exit 1 ;;
esac

CACHE="$ROOT/.libctru"
DIST="$ROOT/3DS"
LOG="$ROOT/build.log"
: > "$LOG"

V134_DIR="$ROOT/Luma3DS_v13_4"
V91_DIR="$ROOT/Luma3DS_v9_1"
# v13.4 は qtmGetSessionHandle() を使う。これは libctru v2.4.0 で追加された。
# 上流は「libctru と Luma3DS のリリースは同期している」としているので、
# v13.4 (2026-04) の直前のリリースである v2.7.0 を既定にする。
# 相性が悪ければ V134_CTRU=v2.6.2 bash build.bat 2 のように上書きできる。
V134_CTRU="${V134_CTRU:-v2.7.0}"
V91_CTRU="${V91_CTRU:-v2.2.2}"

V134_FIRM="$V134_DIR/boot.firm"
V91_FIRM="$V91_DIR/out/boot.firm"

for dir in "$V134_DIR" "$V91_DIR"; do
  if [ ! -f "$dir/Makefile" ]; then
    echo "!! $(basename "$dir")/Makefile が見つかりません。build.bat と同じ場所に $(basename "$dir") フォルダを置いてください。" >&2
    exit 1
  fi
done

STAGE="準備"
trap 'echo; echo "!! ${STAGE}に失敗しました。詳細は build.log を確認してください。" >&2' ERR

if [ "$MODE" = clean ]; then
  STAGE="make clean"
  printf 'v13.4 と v9.1 を make clean 中...'
  for dir in "$V134_DIR" "$V91_DIR"; do
    make -C "$dir" clean >> "$LOG" 2>&1
  done
  echo " 完了"
  trap - ERR
  exit 0
fi

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
    if [ -x "$d/firmtool" ] || [ -x "$d/firmtool.exe" ]; then
      PATH="$PATH:$d"
      break
    fi
  done
fi
export PATH

# 存在確認だけでなく実際に起動する。「PATH にはあるがランタイム DLL が無くて
# 起動できない」ケースは command -v では素通りし、make の中で 127 になって
# 原因が見えなくなるため。出力が 1 バイトでもあればプロセスは動いているので、
# 無出力かつ終了コード 126 以上のときだけ失敗とみなす。
need() {
  local tool="$1" probe="${2:-}" path out rc=0
  path="$(command -v "$tool" 2>/dev/null || true)"
  if [ -z "$path" ]; then
    echo "!! $tool が見つかりません。" >&2
    return 1
  fi
  out="$("$tool" $probe 2>&1)" || rc=$?
  if [ "$rc" -ge 126 ] && [ -z "$out" ]; then
    echo "!! $tool は $path にありますが起動できません（必要な DLL が足りない可能性があります）。" >&2
    return 1
  fi
  printf '%-10s %s\n' "$tool" "$path" >> "$LOG"
}

toolfail=0
need git --version   || toolfail=1
need make --version  || toolfail=1
need makerom         || toolfail=1
need firmtool --help || toolfail=1
if [ "$MODE" != v13.4 ]; then
  need armips --version || toolfail=1
fi
if [ "$toolfail" != 0 ]; then
  echo "必要なツールが揃っていません。armips だけが無いなら、v13.4 は build.bat 2 でビルドできます。" >&2
  exit 1
fi

count_sources() {
  find "$@" -type f \( -name '*.c' -o -name '*.cpp' -o -name '*.s' -o -name '*.S' \) 2>/dev/null | wc -l
}

fetch_libctru() {
  local ver="$1" src="$CACHE/src-$1"
  LIBCTRU_FILES=0
  [ -f "$CACHE/$ver/lib/libctru.a" ] && return 0
  rm -rf "$src"
  git clone --quiet --depth 1 --branch "$ver" https://github.com/devkitPro/libctru "$src" >> "$LOG" 2>&1
  # libctru 自身が -Werror を付けているため、新しい GCC では古い libctru が通らない。
  # ここは依存ライブラリなので警告は警告のまま流す（Luma 本体の HARDERRORS には影響しない）。
  sed -i 's/-Werror//g' "$src/libctru/Makefile"
  LIBCTRU_FILES=$(( 2 * ( $(count_sources "$src/libctru/source") + $(find "$src/libctru/data" -name '*.bin' | wc -l) ) ))
}

build_libctru() {
  local ver="$1" prefix="$CACHE/$1" src="$CACHE/src-$1"
  [ -f "$prefix/lib/libctru.a" ] && return 0
  make -C "$src/libctru" -j"$(nproc)"
  rm -rf "$prefix"
  mkdir -p "$prefix"
  cp -a "$src/libctru/include" "$src/libctru/lib" "$prefix/"
  rm -rf "$src"
}

COMPILED='^[A-Za-z0-9_.+-]+\.(c|cpp|s|S|bin)$|^arm-none-eabi-(gcc|as) .*\.(c|s|S)( |$)'

progress() {
  local label="$1" total="$2" n=0 pct last=-1 line
  while IFS= read -r line; do
    line="${line%$'\r'}"
    [[ $line =~ $COMPILED ]] || continue
    n=$(( n + 1 ))
    pct=$(( n * 100 / total ))
    [ "$pct" -gt 99 ] && pct=99
    [ "$pct" = "$last" ] && continue
    last=$pct
    printf '\r%s をビルド中... %d%%' "$label" "$pct"
  done
}

# make が 0 を返しても成果物が無いことがあるので、実体を必ず確認する。
build_version() {
  local label="$1" dir="$2" ver="$3" target="$4" firm="$5" total
  STAGE="$label のビルド"
  printf '%s をビルド中... 0%%' "$label"
  fetch_libctru "$ver"
  total=$(( LIBCTRU_FILES + $(count_sources "$dir") ))
  {
    trap - ERR
    build_libctru "$ver"
    make -C "$dir" clean || true
    CTRULIB="$CACHE/$ver" STRICT="${STRICT:-0}" GIT_CEILING_DIRECTORIES="$ROOT" make -C "$dir" "$target"
  } 2>&1 | tee -a "$LOG" | progress "$label" "$total"
  if [ ! -s "$firm" ]; then
    echo
    echo "!! $label: boot.firm が生成されていません ($firm)" >&2
    return 1
  fi
  printf '\r%s をビルド中... 100%%\n' "$label"
}

# k11_extension のサイズ = __end__ - ベースVA。
# arm9/source/patches.c は Old 3DS でこのサイズぶんを 0x26C00000 の下、
# BASE メモリ領域から削り取る。ここが膨らむと O3DS だけ起動に失敗しうる。
report_kext() {
  local elf="$1" base="$2" label="$3" end size
  [ -f "$elf" ] || return 0
  end="$("$DEVKITARM/bin/arm-none-eabi-nm" "$elf" | awk '$3 == "__end__" { print $1; exit }')"
  [ -n "$end" ] || return 0
  size=$(( 0x$end - base ))
  printf '%s k11_extension = %d bytes (0x%X)\n' "$label" "$size" "$size" >> "$LOG"
  if [ -n "${K11_MAX:-}" ] && [ "$size" -gt "$((K11_MAX))" ]; then
    echo "!! $label: k11_extension が K11_MAX ($((K11_MAX)) bytes) を超えています" >&2
    exit 1
  fi
}

if [ "$MODE" != v9.1 ]; then
  build_version "v13.4" "$V134_DIR" "$V134_CTRU" all "$V134_FIRM"
  report_kext "$V134_DIR/k11_extension/k11_extension.elf" 0x70000000 "v13.4"
fi

if [ "$MODE" != v13.4 ]; then
  build_version "v9.1" "$V91_DIR" "$V91_CTRU" firm "$V91_FIRM"
  report_kext "$V91_DIR/k11_extension/build/k11_extension.elf" 0x40000000 "v9.1"
fi

STAGE="3DS フォルダの作成"
rm -rf "$DIST"
mkdir -p "$DIST/3ds/luma/13.4" "$DIST/3ds/luma/9.1"
# 既定で起動する版は v13.4。v13.4 が無ければ v9.1 にする。
for f in "$V91_FIRM" "$V134_FIRM"; do
  [ -s "$f" ] && cp "$f" "$DIST/boot.firm"
done
[ -s "$V134_FIRM" ] && cp "$V134_FIRM" "$DIST/3ds/luma/13.4/boot.firm"
[ -s "$V91_FIRM" ] && cp "$V91_FIRM" "$DIST/3ds/luma/9.1/boot.firm"

{
  echo
  echo "=== 要確認の警告（-fpermissive が隠していた型の問題） ==="
  grep -hoE "\[-W(int-conversion|incompatible-pointer-types|implicit-function-declaration|implicit-int|return-mismatch|return-type)[^]]*\]" "$LOG" \
    | sort | uniq -c | grep . || echo "なし"
} >> "$LOG" 2>&1

trap - ERR
echo
echo "完了しました。3DS フォルダの中身を SD カードのルートにコピーしてください。"
