# 改変に関する告知 (Modification Notice) — v13.4

本ソフトウェアは Luma3DS v13.4 を改変したものです。

- 原著作物: Luma3DS (C) Aurorawright, TuxSH, LumaTeam および貢献者
- 日本語フォントデータ (font_jp.h): Luma3DS 3GX フォーク（改変者: ELVES ほか）由来
- ライセンス: GNU General Public License v3.0（同梱の LICENSE）
- 本改変者: ELVES

原著作権表示およびライセンスは保持しています。追加の制限は課していません。本改変部分も GPLv3 に従います。

## 改変履歴 (Change History)

### 2026-07-06 —ELVES
- バージョン切替チェインローダーを追加（v9.1 と共通の状態ファイル /luma/chainload_target・フラグ /luma/chainload_ver.flag を使用）。
- Rosalina に日本語表示機能を移植（UTF-8 デコードと日本語グリフ描画、font_jp.h）。
- 起動時に自分の版を状態ファイルへ記録し、切替メニューが現在版を正しく判定できるよう修正。
- 書き込み失敗時は再起動せずエラー表示するよう修正。
- チェインロードフラグを CFG_BOOTENV の強制ブート処理より前で確認。
- 新しい GCC でビルドするためのビルド設定を変更。

### 2026-07-31 — ELVES
- ビルドフラグを修正。C では危険な診断（暗黙の関数宣言・非互換ポインタ・int/ポインタ変換）を
  格下げしてしまう `-fpermissive` を全 Makefile から削除し、`-Wno-error` を廃止。
  それらを常にエラーにする `HARDERRORS` と、全警告をエラーにする `STRICT=1` を追加した。
- v9.1 の 2026-07-11 の修正を移植。チェインロード失敗時に無言で現行版を起動せず、
  エラーを表示して停止する。
- 切替先 boot.firm の存在確認 (`chainloadVersionFirmExists`) がサイズしか見ていなかったのを、
  FIRM マジックも確認するよう修正。
- git 管理外で `git describe` がエラー出力するのを抑制。

### 2026-08-01 — ELVES
- `FindPluginFile()` が最初に見つけた .3gx を無条件に採用していたのを、
  `Check_3gx_Magic()` に通してから採用するよう変更。世代が合わないファイルは
  読み飛ばして次の候補を探す。同じフォルダに 0001 世代と 0002 世代が混在していても、
  読める方が選ばれる。
  すべて世代違いだった場合は "Outdated plugin file" を返し、
  そもそも .3gx が無かった場合と区別する。

### 2026-08-01 (2) — ELVES
- Rosalina のメニューと主要メッセージを日本語化（164 箇所）。
  メインメニュー、デバッガ設定、その他の設定、New 3DS 設定、画面フィルタ、
  システム設定、プロセス一覧、チート、電源、システム情報など。
- `font_jp.h` を再生成（330 文字）。v9.1 の `tools/genfont_jp.py` と `b10.bdf` を流用。
- `process_list.c` の重ね描き座標を訳文の文字位置に合わせて修正。

### 2026-08-01 (3) — ELVES
- 表示言語を英語／日本語で切り替えられるようにした（`i18n.c` / `i18n.h`）。
  ソースの文字列は英語のまま、表示時に `TR()` を通す方式。選択は
  `SD:/luma/rosalina_lang.txt` に保存し、v9.1 と共有する。
- Rosalina メニューに「Language」を追加。
- `process_list.c` の重ね描き位置を `I18n_Columns()` で実文字列から計算するようにした。

### 2026-08-01 (4) — ELVES
- `loadVersionFirm()` が目標＝自分自身のときにチェインロードしないようにした（v9.1 と同じ）。

### 2026-08-01 (5) — ELVES
- 状態ファイルを `/luma/chainload_ver.flag` の 1 本に統合（v9.1 と同じ）。
  毎起動の `chainload_target` 書き込みを廃止。

### 2026-08-01 (6) — ELVES
- 日本語化を撤回し、表示は英語一本にした。`draw.c` は上流と完全一致に戻した。
- **3gx セレクターを v9.1 から移植**（`plugin/file_loader.c`）。
  複数のプラグインがある場合に一覧から選択でき、X で記憶できる。
  起動時に X = セレクターを出さず default.3gx、B = 記憶を消して一覧、
  Y = 記憶を無視して一覧。対応しない世代は赤字で選択不可。
- **破壊 3gx チェッカーを v9.1 から移植**。読み込み前に内容を走査し、
  危険な可能性があれば使用の可否を尋ねる。
- `draw.h` に `COLOR_YELLOW` を追加（セレクターの記憶表示用）。

### 2026-08-01 (7) — ELVES
- `menus.c` の `#include <string.h>` を削除（`memory.h` が既に取り込んでいる）。
- バージョン切替画面に残っていた `targetIs13` の参照を修正。

### 2026-08-02 — ELVES
- **配列外書き込みの修正。** `utf16_to_utf8()` は出力バッファに収まらなくても -1 を返さず、
  「本来必要だった長さ」を返す（libctru の実装で確認）。返り値をそのまま
  `g_pluginEntries[n][units] = 0` の添字に使っていたため、長いファイル名で
  配列外に書き込む可能性があった。`FS_DirectoryEntry.name` は 262 UTF-16 単位なので
  UTF-8 では最大 783 バイトになりうる（行は 256 バイト）。
  上限を明示し、超える名前は候補から外すようにした。上流 Luma3DS にも同じ形が残っている。
- `sprintf` に 2 次元配列の要素を直接渡すのをやめ、長さの分かる局所配列を経由するようにした
  （`-Wformat-overflow` の警告対策）。
- `IsPluginLoadable()` のパスバッファを `g_path` と名前の合計に合わせた。

## 改変したファイル (Modified Files)

- arm9/source/firm.c, arm9/source/firm.h
- arm9/source/main.c
- sysmodules/rosalina/source/menus.c
- sysmodules/rosalina/source/plugin/file_loader.c
- sysmodules/rosalina/include/menus.h
- sysmodules/rosalina/source/draw.c
- sysmodules/rosalina/include/font_jp.h (新規)
- arm9/Makefile

## ビルド要件

- devkitARM / libctru 2.3.x 以降 / armips / makerom / firmtool
