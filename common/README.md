# common

改変版（boot.firm_Customize-Model）が、新しい方の Luma（`Luma3DS_new`）と v9.1（`Luma3DS_v9_1`）の両方で使うコードです。
同じものを 2 か所に持たないよう、ここに 1 つだけ置いて、両方の Makefile から参照しています。
上流のファイルではないので、`tools/upgrade_upstream.sh` で上流の版を上げても変わりません。

| フォルダ | 中身 | 使うところ |
|---|---|---|
| rosalina/source、rosalina/include | Rosalina に足した機能（下の表） | 両方の版の `sysmodules/rosalina/Makefile` |
| bearssl | BearSSL 0.6 のうち使う分（bearssl/README.txt） | 両方の版の Rosalina と、更新アプリ（`updater/Makefile`） |

## rosalina

| ファイル | 中身 |
|---|---|
| include/fork.h | 改変版の版（`FORK_PACKAGE_TAG`。build.bat もここから読む）と、共通の関数の宣言 |
| source/menus/fork_menus.c | バージョン切り替え、ログ（/luma/log.txt）、SD カードの空き容量の警告、ファイルブラウザー |
| source/menus/ftpd.c | FTP サーバー |
| source/menus/lang.c、include/menus/lang.h | 画面の言葉（`TR("日本語", "English")`。v1.5 からは英語のほうだけを使う）と、その表 |
| source/menus/text_input.c | 文字入力 |
| source/font_jp.c、include/font_jp.h | 日本語の字（boot.firm の後ろに付けたフォントと、埋め込みの字形） |
| include/font_jp_ui.h | 埋め込みの字形。`font_jp/mkfont_jp_ui.py` で作る（手で直さない） |
| source/update_check.c、source/update_tls.c | 起動時の新しい版の確認（GitHub に HTTPS で問い合わせる） |

## 版ごとの違いの扱い

ここのコードは、新しい方の Rosalina の名前（`KEY_B`、`menuShouldExit`、`socSend` など）で書いてあります。
v9.1 では名前が違うので、`Luma3DS_v9_1/sysmodules/rosalina/include/fork_compat.h` が v9.1 の名前に読み替えます
（新しい方にも同じ名前のヘッダーがあり、こちらはほとんど空です）。版ごとに中身が違うもの
（`FORK_LUMA_ID`、`chainloadVersionFirmExists` など）は、それぞれの版の `menu.h`・`menus.c` に残しています。

ここのファイルを変えたら、両方の版をビルドして確かめます（`build.bat 1`）。
