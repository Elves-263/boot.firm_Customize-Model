# 改変に関する告知 (Modification Notice) — v9.1

本ソフトウェアは Luma3DS を改変したものです。

- 原著作物: Luma3DS (C) Aurorawright, TuxSH, LumaTeam および貢献者
- 3GX プラグインローダー: (C) PabloMK7, Nanquitas ほか
- 本配布物のベース: Luma3DS 3GX フォーク（既存の改変者: ELVESほか）
- ライセンス: GNU General Public License v3.0（同梱の LICENSE.txt）
- 本改変者: ELVES

原著作権表示およびライセンスは保持しています。追加の制限は課していません。本改変部分も GPLv3 に従います。

## 改変履歴 (Change History)

### 2026-07-06 — ELVES
- バージョン切替チェインローダーを追加。Rosalina メニューの「バージョン切替」から SD 上の別バージョン boot.firm を起動する。状態ファイル /luma/chainload_target とフラグ /luma/chainload_ver.flag により v9.1 と v13.4 を相互に切り替える。
- 起動時に自分の版を状態ファイルへ記録するようにし、切替メニューが現在版を正しく判定できるよう修正。
- 書き込み失敗時は再起動せずエラー表示するよう修正。
- チェインロードフラグの処理を CFG_BOOTENV の強制ブート処理より前へ移動（再起動時に確実に発火）。
- クレジット文字列内の不正な制御文字を除去。
- 表示バージョンを v9.1.0 に固定（Makefile）。
- 新しい devkitARM / libctru でビルドするためのビルド設定を変更。

### 2026-07-11 — ELVES
- Old 3DS でのフリーズを修正。Rosalina メニューが OS を凍結したまま APT へ IPC を発行していたため、menuLeave() で凍結を解除してから APT_HardwareResetAsync() を呼ぶよう順序を変更した。バージョン切替・alt-boot 起動・再起動の 3 箇所に適用。
- チェインロード失敗時に無言で現行版が起動していた問題を修正。切替先 boot.firm が不在または破損している場合はエラーを表示して停止する。

### 2026-07-31 — ELVES
- ビルドフラグを修正。C では危険な診断（暗黙の関数宣言・非互換ポインタ・int/ポインタ変換）を
  格下げしてしまう `-fpermissive` を全 Makefile から削除し、それらを常にエラーにする
  `HARDERRORS` を追加。全警告をエラーにする `STRICT=1` を用意した。
- 切替先 boot.firm の存在確認 (`chainloadVersionFirmExists`) がサイズしか見ていなかったのを、
  FIRM マジックも確認するよう修正。
- ビルドスクリプトを整理し、k11_extension のサイズを表示するようにした
  （Old 3DS では BASE メモリ領域から削り取られるため、肥大すると O3DS だけ起動に失敗する）。

### 2026-08-01 — ELVES
- 3gx の世代チェックを追加。`Check_3gx_Magic()` を `3gx.c` に実装し、
  `PluginLoader__Load()` で `Read_3gx_Header()` の前に呼ぶようにした。
  これまで magic を一切見ずにヘッダを読んでいたため、0002 世代 (0.7.x 系) の 3gx を
  読ませると壊れたオフセットを掴んで fs プロセスが飛んでいた。
  上流 v13.4 と同じロジック・同じエラー文言。

### 2026-08-01 (2) — ELVES
- 3gx セレクターで世代の合わないプラグインを選べないようにした。
  `EnumeratePluginFiles()` で各ファイルの先頭を読んで `Check_3gx_Magic()` にかけ、
  結果を `g_pluginEntryOk[]` に保持する。セレクターでは非対応を赤字 + "(incompatible)"
  で表示し、カーソルが乗らないようにした。
  1件だけのときの自動選択と、`.selected3gx` に記憶した選択も対応版に限定。
  フォルダ内が全部非対応なら、そもそもプラグインを読み込まない。

### 2026-08-01 (3) — ELVES
- Rosalina の未翻訳だった表示を日本語化（プラグイン選択、メモリビューア、チート、
  エラー表示など 40 箇所）。
- `font_jp.h` を再生成（153 → 342 文字）。生成には既存の `tools/genfont_jp.py` を使用。
  両ツリーまとめて回す `tools/gen_font_jp.sh` を追加した。
- `process_list.c` の重ね描き座標を訳文の文字位置に合わせて修正。

### 2026-08-01 (4) — ELVES
- 表示言語を英語／日本語で切り替えられるようにした（`i18n.c` / `i18n.h`）。
  ソースの文字列は英語に戻し、表示時に `TR()` を通す方式。選択は
  `SD:/luma/rosalina_lang.txt` に保存し、次回起動時に読む。
- Rosalina メニューに「Language」を追加。
- 以前から日本語だった箇所も対訳表に移した（英語原文は上流 v9.1 から復元）。
- `process_list.c` の重ね描き位置を `I18n_Columns()` で実文字列から計算するようにし、
  英語・日本語のどちらでもずれないようにした。

### 2026-08-01 (5) — ELVES
- `loadVersionFirm()` が目標＝自分自身のときにチェインロードしないようにした。
  `SD:/boot.firm` がこの版そのものだと、同じ FIRM を自分の上に読み直すことになり
  意味が無いうえ、途中で止まると画面が出ないまま固まる。

### 2026-08-01 (6) — ELVES
- 状態ファイルを `/luma/chainload_ver.flag` の 1 本に統合。中身が目標バージョン文字列。
  ARM9 が毎起動 `chainload_target` を書いていたのを廃止し、SD への書き込みを
  「切り替え実行時の 1 回」だけにした（SD 破損の機会を減らすため）。
- Rosalina は現在版をコンパイル時定数 `MY_LUMA_VERSION` で持つようにし、
  現在版の判定で SD を読まないようにした。

### 2026-08-01 (7) — ELVES
- 3gx セレクターで X を押しながら起動すると、セレクターを開かず
  `/luma/plugins/default.3gx` を使うようにした。

### 2026-08-01 (8) — ELVES
- 日本語化を撤回し、表示は英語一本にした。`i18n.c` / `i18n.h` / `i18n_table.h` /
  `font_jp.h` と生成ツールを削除し、`draw.c` を上流 v9.1 に戻した
  （GCC14 で通らない `(u32)` キャストのみ再適用）。
- 破壊 3gx チェッカーの検査前の画面フラッシュを削除（v13.4 と挙動を揃えるため）。

### 2026-08-01 (9) — ELVES
- `draw.c` を上流 v9.1 に戻した際に、libctru の型名変更への追従
  (`GSPGPU_FramebufferFormats` → `GSPGPU_FramebufferFormat`) まで巻き戻していたのを修正。
  上流 v9.1 は 2018 年のコードなので旧名のまま。新しい libctru では通らない。

### 2026-08-01 (10) — ELVES
- `menus.c` に足していた `#include <string.h>` を削除。Luma の `memory.h` が
  memcpy/memcmp/strlen などを独自シグネチャ (u32) で宣言しており、newlib の
  string.h と衝突する。これらは memory.h 側で揃うので string.h は不要。
- バージョン切替画面に残っていた `targetIs13` の参照を `MY_LUMA_VERSION` / `target` に修正。

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

### 2026-08 ～ 2026-10（配布版 v1.0 ～ v1.4）— ELVES
配布版ごとの変更の詳細は、配布物の README.txt の「更新履歴」を参照。主な改変は次のとおり。
- v1.0: 3gx セレクター、破壊的 3gx チェッカー、3gx / txt チェッカー、配色の設定（Colors）、
  セルフテスト、SD 空き容量の警告、エラーログ（/luma/log.txt）、ファイルブラウザ（閲覧のみ）、
  セーフモード（L + R + A）を追加。
- v1.0_fix: メニューのスクロール表示、3gx の拡張子判定の修正、破壊的 3gx の承認の記録を廃止。
- v1.1: バッテリー情報、メモリダンプ、ファイル操作、FTP サーバー、表示言語の切り替えを追加
  （`sysmodules/rosalina/source/menus/` 以下の battery.c、ftpd.c、lang.c、memdump.c、text_input.c）。
  バージョン切り替えで SD:/boot.firm を差し替えるようにし、切り替え時に画面を毎回初期化。
  破壊的 3gx の判定をパスの形と重み付けに変更。
- v1.2: v9.1 の追加モジュールの大きさの書き込みを修正（Old 3DS で起動しない件）、
  ファイルのコピーで使う作業用メモリ（スタック）を削減。
- v1.3: Check SD Card を追加（sha256.c）。boot.firm を一時ファイルで確かめてから差し替える。
  破壊的 3gx の検査をプログラム部分に限定し、承認を /luma/plugins/.approved3gx に記録。
  v9.1 の、ファイルの末尾で読み込みを繰り返す不具合を修正（ifile.c）。
- v1.4: FTP サーバーの作り直し（同時接続、日本語名、更新日時、REST、ABOR）、
  ファイルブラウザの並べ替え・中身の表示・UTF-16 の名前、バッテリー容量の設定、
  メニューと FTP のメモリを使うときだけ確保、Wi-Fi の状態確認で ac の接続を閉じる（sock_util.c）、
  消えていた本家のコメントを復元。あわせて、FTP の上書きでの元ファイルの消失、
  3gx セレクターの B で default.3gx が読まれる件、メニュー表示中にバッテリーを記録しない件、
  boot.firm の差し替え失敗を知らせない件などを修正し、改変版の画面の文字列をローマ字表示に対応。
  チートを、ゲームの起動時に自動で有効にする機能を追加（menus/cheats.c）。
  メニュー・3gx セレクター・Memory dump の一覧で、十字左右で 1 画面ずつ動けるようにした（menu.c、plgloader.c、menus/memdump.c）。
  十字キーを押し続けたときに続けて動くようにした（menu.c。v13.4 と同じ間隔）。
  日本語の表示を追加（font_jp.c / font_jp.h、draw.c、menus/lang.c ほか）。字形は M+ BITMAP FONTS の
  10 ドットの字から作った /luma/font_jp.bin（作るスクリプトは配布ソースの font_jp/mkfont_jp.py）を
  SD カードから読む。チート名（UTF-8 / Shift-JIS）、ファイル名、改変版の画面の文字を漢字・かなで表示し、
  ファイルが無いときは従来どおり ? とローマ字で表示する。
- v1.4（v9.1 のみ）: PXI の終了処理を本家 Luma3DS（2020 年の修正）に合わせた。
  メニュー用のメモリを確保できないときも、致命的なエラーの画面を表示する（errdisp.c）。
  cheats.txt の読み込みで、チートを入れる領域（16KB）の残りを確かめるようにした（menus/cheats.c）。

### 2026-10-23 — ELVES
- 3gx セレクター・破壊的 3gx の警告を表示している間は、Rosalina メニューを開かないようにした。
  Rosalina メニューを開いている間にこれらを出すときは、閉じるまで待つ。これまでは 2 つの画面が
  同時に動き、同じボタン入力を両方が受け取っていた。
- フォルダ内の .3gx がすべて世代違いのとき、default.3gx に切り替えていたのを、
  先頭のファイルを開いて "Outdated plugin file" などを表示するようにした。
- ファイルブラウザの上書き貼り付けで、コピーに失敗すると元のファイルが消えていたのを修正した。
  一時ファイルに書き終えてから、元のファイルを退避して差し替える。

### 2026-10-24 — ELVES
- Colors、Language / Gengo、3gx / txt checker、Check SD Card、Battery info、Memory dump を削除した
  （battery.c、memdump.c、sha256.c とそのヘッダも削除）。画面の色は本家と同じ固定の色に戻した。
- 表示を日本語（漢字・かな）に固定した。/luma/lang.cfg は読まない。
- 改変版の画面で使う字の字形を `font_jp_ui.h` として埋め込んだ（`font_jp/mkfont_jp_ui.py` で
  font_jp.bin から作る）。/luma/font_jp.bin が無くても、改変版の画面は漢字・かなで出る。
  font_jp.bin はチート名・ファイル名の漢字と、Shift-JIS の cheats.txt を読むときにだけ使う（無ければ ?）。
- バージョン切り替えの失敗の知らせにあった「Check SD Card で確認してください」を
  「SD カードを確認してください」にした。

### 2026-10-24 (2) — ELVES
- Rosalina の表示を、エラーの知らせを除いて日本語にした（メニューの項目、画面のタイトル、
  操作の案内、うまくいったときの知らせ、注意書き、クレジットなど）。日本語と英語を並べていた
  知らせ（破壊的 3gx の警告、SD の空き容量の警告、キーの案内）は日本語だけにした。
- 失敗したことやできなかったことを知らせる文（"Operation failed (0x...)"、"Couldn't read file" など）、
  /luma/log.txt に書く文、エラー画面（errdisp.c）、HOME ボタンが使えないときの知らせ
  （DispWarningOnHome）は変えていない。ボタンの名前（メニューを開くボタンの表示）と、
  InputRedirection などの名前も英語のまま。
- メニューの項目を、バイト数ではなく画面の桁数で空白を埋めるようにした（menu.c）。
  タイトルの右の版の表示の位置も桁数で決める。セーフモードの表示は「セーフ」にした。
- 日本語にした画面の位置と幅を桁数で数えるようにした（ファイルブラウザの見出しとクリップボードの行、
  FTP の転送中の行、メモリビューアーの案内の強調、プロセス一覧の注記の大きさ）。
- 画面で使う字が増えたので `font_jp_ui.h` を作り直した。

### 2026-10-24 (3) — ELVES
- 起動時に新しい版を確かめるようにした（update_check.c）。Wi-Fi に繋がったあと 1 度だけ、
  GitHub の最新のリリースの名前を取る。動いている版（menu.h の FORK_PACKAGE_TAG）より新しければ、
  Rosalina メニューを開いたとき（起動ごとに 1 度）とお知らせリスト（1 つの版につき 1 度）で知らせる。
  確かめる先は build.bat の UPDATE_REPO で、空ならこの機能は動かない。セーフモードでは確かめない。
- 3DS 本体の HTTPS は TLS 1.1 までで GitHub に繋がらないため、BearSSL 0.6（MIT、source/bearssl/）で
  TLS 1.2 を話す（update_tls.c）。版の名前だけを取るので証明書は確かめない。ダウンロードは
  更新アプリ（配布ソースの updater/）が証明書と SHA-256 を確かめて行う。
- minisoc.c に名前の解決（socGethostbyname）を足した。socConnect も v13.4 から写した。
- 確かめた結果などを /luma/update/ に書く（running.txt・check.txt・notified.txt。updater/README.md を参照）。

### 2026-11-09 — ELVES
- 新しい方の Luma を上流 v13.5 に上げたのにあわせ、相手の版を「new」と呼ぶようにした。SD カードの
  フォルダは /3ds/luma/new/、/luma/chainload_ver.flag と /luma/update/running.txt に書く名前は "new" と "9.1"
  （firm.c、menu.h、menus.c、update_check.c）。「バージョン切り替え」に出す相手の版は、ビルドのときに
  Luma3DS_new/UPSTREAM_VERSION から取る（Makefile の `LUMA_NEW_VERSION`）。
- 表示の言語を英語と日本語で切り替えられるようにした（Rosalina の「言語 / Language」）。選んだ言語は
  /luma/lang.cfg（'0' が英語、'1' が日本語）に書き、新しい方の Luma と共有する。lang.cfg が無いときは
  本体の言語が日本語なら日本語、それ以外は英語にして、lang.cfg に書く。2026-10-24 の
  「/luma/lang.cfg は読まない」は取りやめた。文字列は `TR("日本語", "English")`（include/menus/lang.h）で書く。
  エラーの知らせは今までどおり（日英併記）。
- 字形（font_jp.bin）を SD カードから読むのをやめ、boot.firm の後ろに付け足したものを読むようにした（font_jp.c）。
- 起動時の Luma の設定画面を日本語にした（source/config.c、pin.c、fs.c、draw.c、draw.h、lang.c / lang.h、
  font_jp_ui.h）。言語は /luma/lang.cfg に従い、無ければ日本語。エラーの画面は英語のまま。
- 更新アプリのダウンロードで SHA-256 を確かめるのをやめた（2026-10-24 (3) の記述を参照。大きさを確かめる）。

## 改変したファイル (Modified Files)

本配布物のベース（Luma3DS 3GX フォーク）から ELVES が変更したファイル（2026-11 時点）。

- 追加したファイル
  - source/lang.c, source/lang.h, source/font_jp_ui.h
  - sysmodules/rosalina/source/menus/ftpd.c, lang.c, text_input.c
  - sysmodules/rosalina/include/menus/ftpd.h, lang.h, text_input.h
  - sysmodules/rosalina/source/font_jp.c, sysmodules/rosalina/include/font_jp.h, font_jp_ui.h
  - sysmodules/rosalina/source/update_check.c, update_tls.c, sysmodules/rosalina/include/update_check.h,
    update_tls.h、sysmodules/rosalina/source/bearssl/（BearSSL 0.6 の一部）
- 変更したファイル
  - source/firm.c, source/firm.h, source/main.c, source/fs.c, source/fs.h, source/fmt.c,
    source/patches.c, source/buttons.h, source/config.c, source/draw.c, source/draw.h, source/pin.c
  - sysmodules/rosalina/source/menus/cheats.c, sysmodules/rosalina/include/menus/cheats.h
  - sysmodules/rosalina/source/menus/debugger.c, miscellaneous.c, n3ds.c, process_list.c,
    sysconfig.c（表示の日本語化）
  - sysmodules/rosalina/source/main.c, menu.c, menus.c, draw.c, errdisp.c, fmt.c, ifile.c,
    sock_util.c, minisoc.c, 3gx.c, plgloader.c, gdb/debug.c
  - sysmodules/rosalina/include/3gx.h, draw.h, ifile.h, menu.h, menus.h, minisoc.h
  - sysmodules/pxi/source/main.c
  - Makefile、k11_extension/Makefile、sysmodules/*/Makefile ほか各 Makefile（ビルド設定）

## ビルド要件

- devkitARM / libctru v2.2.2（build.bat が .libctru に用意する）/ armips / makerom / firmtool
