# 上流の新しい版に上げる

新しい方の Luma（`Luma3DS_new`）は、上流の LumaTeam/Luma3DS のある版に改変版の変更を載せたものです。
どの版に載せているかは `Luma3DS_new/UPSTREAM_VERSION`（例: `v13.5`）の 1 か所だけに書いてあり、
画面に出す版、arm9 の版、Rosalina の「バージョン切り替え」、更新アプリ、build.bat はここから取ります。
SD カードのフォルダは版によらず `/3ds/luma/new/` なので、版を上げても変わりません。

改変版が新しい方と v9.1 の両方で使うコード（FTP サーバー、ファイルブラウザー、バージョン切り替え、
起動時の更新確認など）は `Luma3DS_new` の外の `common/` にあるので、上流の版を上げても変わりません
（`common/README.md` を参照）。

## 上げ方

Linux か devkitPro の MSYS2 で、このフォルダのひとつ上（build.bat のあるところ）から実行します。

```
bash tools/upgrade_upstream.sh v13.6
```

1. 上流から今の版（UPSTREAM_VERSION）と新しい版を取ってきます（`.upgrade/luma` に置きます）。
2. 今の版の上に改変版の中身を載せ、それを新しい版の上に 3-way マージで載せ直します。
3. ぶつかるところが無ければ、そのまま `Luma3DS_new` を新しい中身にして、UPSTREAM_VERSION を書き換えます。

ぶつかるところがあると、そのファイルの名前を出して止まります。`.upgrade/luma` の中のそのファイルで、
`<<<<<<<` から `>>>>>>>` の間（上が上流、下が改変版）を直してから、仕上げます。

```
bash tools/upgrade_upstream.sh --finish
```

やめるときは `--abort` です（`Luma3DS_new` は変わりません）。

## 上げたあとにすること

1. **libctru の版を見直す。** build.bat の `NEW_CTRU` を、上流の版と同じころの libctru のリリースにします
   （上流は「libctru と Luma3DS のリリースは同期している」としています）。
2. **新しい英語の文字列を日本語にする。** 上流で増えた画面の文字列は英語のままです。次で探せます。
   ```
   grep -rnP '(Draw_Draw\w*\(|\{ ")[^;]*"[^"]*[A-Za-z]{3,}' Luma3DS_new/sysmodules/rosalina/source | grep -v 'TR('
   grep -rn 'drawString\|drawFormattedString' Luma3DS_new/arm9/source | grep -v 'TR('
   ```
   Rosalina は `TR("日本語", "English")`（`common/rosalina/include/menus/lang.h`）、arm9 は
   `TR("日本語", "English")`（`arm9/source/lang.h`）で書きます。エラーの文は英語のままにします（Rosalina のエラーは日英併記）。
3. **字形を作り直す。** 日本語の文字列を変えたら、埋め込む字形を作り直します。
   ```
   python3 font_jp/mkfont_jp_ui.py
   ```
4. **書式を確かめる。** 日本語と英語で `%d` などの並びが同じか確かめます（GCC が検査するのは、使うほうの言語だけです）。
   ```
   python3 font_jp/chk_tr.py Luma3DS_new/sysmodules/rosalina/source Luma3DS_v9_1/sysmodules/rosalina/source \
                             common/rosalina/source Luma3DS_new/arm9/source Luma3DS_v9_1/source updater/source
   ```
5. **共通のコードが使う名前を確かめる。** `common/rosalina` は新しい方の Rosalina の名前（`menuShouldExit`、
   `socSend` など）で書いてあります。上流がその名前を変えたら、`common/rosalina` のほうを新しい名前に直し、
   `Luma3DS_v9_1/sysmodules/rosalina/include/fork_compat.h` の読み替えを合わせます。
6. **ビルドして確かめる。** `build.bat 1` が最後まで通ることと、実機で起動・版の切り替え・Rosalina の
   メニューを確かめます。
7. **更新.md に書く。** 上流の版を上げたことを、次の版の節に書きます。
