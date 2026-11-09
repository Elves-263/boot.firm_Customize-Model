# 改変に関する告知 (Modification Notice)

本ソフトウェアは Luma3DS を改変したものです。もとにした上流の版は同じフォルダの UPSTREAM_VERSION に書いてあります
（2026-11 時点で v13.5）。

- 原著作物: Luma3DS (C) Aurorawright, TuxSH, LumaTeam および貢献者
- ライセンス: GNU General Public License v3.0（同梱の LICENSE）
- 本改変者: ELVES

原著作権表示およびライセンスは保持しています。追加の制限は課していません。本改変部分も GPLv3 に従います。

## 改変履歴 (Change History)

### 2026-07-06 —ELVES
- バージョン切替チェインローダーを追加（v9.1 と共通の状態ファイル /luma/chainload_target・フラグ /luma/chainload_ver.flag を使用）。
- Rosalina に日本語表示機能を移植（UTF-8 デコードと日本語グリフ描画、font_jp.h）。
  （のちに撤回。v1.4 で、字形を SD カードの /luma/font_jp.bin から読む形で改めて追加した）
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
  メニュー・3gx セレクター・Memory dump の一覧で、十字左右で 1 画面ずつ動けるようにした（menu.c、plugin/file_loader.c、menus/memdump.c）。
  日本語の表示を追加（font_jp.c / font_jp.h、draw.c、menus/lang.c ほか）。字形は M+ BITMAP FONTS の
  10 ドットの字から作った /luma/font_jp.bin（作るスクリプトは配布ソースの font_jp/mkfont_jp.py）を
  SD カードから読む。チート名（UTF-8 / Shift-JIS）、ファイル名、改変版の画面の文字を漢字・かなで表示し、
  ファイルが無いときは従来どおり ? とローマ字で表示する。

### 2026-10-23 — ELVES
- 3gx セレクター・破壊的 3gx の警告を表示している間は、Rosalina メニューを開かないようにした。
  Rosalina メニューを開いている間にこれらを出すときは、閉じるまで待つ。これまでは重ねて開けて、
  `menuEnter()` の数が合わなくなり、以降メニューが表示されなくなることがあった。
- `menuEnter()` は画面を開いたときだけ true を返すようにした。セレクター・警告を表示できないとき
  （メモリ不足など）は、見えないまま入力を待たずに「使わない」と同じ扱いにする。
- フォルダ内の .3gx がすべて世代違いのとき、何も出さずに default.3gx に切り替えていたのを、
  本家と同じく先頭のファイルを開いて "Outdated plugin file" などを表示するようにした（2026-08-01 の記述どおり）。
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
  /luma/log.txt に書く文、エラー画面（errdisp.c）は変えていない。ボタンの名前（メニューを開くボタンの
  表示など、config.ini と同じ書き方のもの）と、InputRedirection などの名前も英語のまま。
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
  更新アプリ（配布ソースの updater/）が証明書を確かめて行う（v1.4 で SHA-256 の確認はやめ、大きさを確かめる）。
- minisoc.c に名前の解決（socGethostbyname）を足した。
- 確かめた結果などを /luma/update/ に書く（running.txt・check.txt・notified.txt。updater/README.md を参照）。

### 2026-11-09 — ELVES
- もとにする上流の版を v13.4 から v13.5 に上げた。改変版の変更を v13.4 の上に 1 つのコミットとして載せ、
  v13.5 の上に 3-way マージで載せ直した。上流の版は UPSTREAM_VERSION の 1 か所にだけ書き、
  画面の版の表示、arm9 の版、Rosalina の「バージョン切り替え」、build.bat はここから取る（Makefile の
  `LUMA_NEW_VERSION`）。次の版に上げる手順は配布ソースの tools/README.md と tools/upgrade_upstream.sh。
- フォルダ名を Luma3DS_v13_4 から Luma3DS_new に、SD カードのフォルダを /3ds/luma/13.4/ から
  /3ds/luma/new/ にした。/luma/chainload_ver.flag と /luma/update/running.txt に書く名前も "new" にした
  （版を上げても変わらない）。
- 表示の言語を英語と日本語で切り替えられるようにした（Rosalina の「言語 / Language」）。選んだ言語は
  /luma/lang.cfg（'0' が英語、'1' が日本語）に書く。lang.cfg が無いときは本体の言語が日本語なら日本語、
  それ以外は英語にして、lang.cfg に書く。2026-10-24 の「/luma/lang.cfg は読まない」は取りやめた。
  文字列は `TR("日本語", "English")`（include/menus/lang.h）で書き、描画（draw.c）と書式（fmt.c）で
  言語を選ぶ。エラーの知らせは今までどおり（日英併記）。
- 字形（font_jp.bin）を SD カードから読むのをやめ、boot.firm の FIRM のセクションの後ろ
  （0x200 にそろえた位置）に付け足したものを読むようにした（font_jp.c）。build.bat が付け足す。
- 起動時の Luma の設定画面（arm9）を日本語にした（config.c、pin.c、fs.c、lang.c / lang.h）。
  draw.c は UTF-8 を読み、漢字・かなを `font_jp_ui.h` の字形で描く（全角 12 ドット、半角 6 ドット）。
  言語は /luma/lang.cfg に従い、無ければ日本語。arm9 のエラーの画面は英語のまま。
- 上流 v13.5 で増えた画面（システム設定、画面フィルタ、その他の設定の温度の単位）を日本語にした。
  日付の表示を日本の書き方にした（utils.c）。
- 新しい版を知らせる文の題を言語に合わせた（update_check.c）。

## 改変したファイル (Modified Files)

本家 Luma3DS（UPSTREAM_VERSION の版）から変更したファイル（2026-11 時点）。

- 追加したファイル
  - UPSTREAM_VERSION
  - arm9/source/lang.c, lang.h, font_jp_ui.h
  - sysmodules/rosalina/source/menus/ftpd.c, lang.c, text_input.c
  - sysmodules/rosalina/include/menus/ftpd.h, lang.h, text_input.h
  - sysmodules/rosalina/source/font_jp.c, sysmodules/rosalina/include/font_jp.h, font_jp_ui.h
  - sysmodules/rosalina/source/update_check.c, update_tls.c, sysmodules/rosalina/include/update_check.h,
    update_tls.h、sysmodules/rosalina/source/bearssl/（BearSSL 0.6 の一部）
- 変更したファイル
  - arm9/source/firm.c, arm9/source/firm.h, arm9/source/main.c, arm9/source/fs.c, arm9/source/fs.h,
    arm9/source/fmt.c
  - arm9/source/config.c, draw.c, draw.h, pin.c（設定画面の日本語化）
  - sysmodules/rosalina/source/menus/cheats.c, sysmodules/rosalina/include/menus/cheats.h
  - sysmodules/rosalina/source/menus/debugger.c, miscellaneous.c, n3ds.c, process_list.c,
    screen_filters.c, sysconfig.c（表示の日本語化）
  - sysmodules/rosalina/source/main.c, menu.c, menus.c, draw.c, fmt.c, ifile.c, sock_util.c, minisoc.c, utils.c
  - sysmodules/rosalina/source/plugin/file_loader.c, sysmodules/rosalina/source/plugin/plgloader.c,
    sysmodules/rosalina/source/plugin/display.c
  - sysmodules/rosalina/include/draw.h, ifile.h, menu.h, menus.h, minisoc.h
  - Makefile、arm9/Makefile、arm11/Makefile、k11_extension/Makefile、sysmodules/*/Makefile（ビルド設定）

## ビルド要件

- devkitARM / libctru v2.7.1（build.bat が .libctru に用意する）/ makerom / firmtool
