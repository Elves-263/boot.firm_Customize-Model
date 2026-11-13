# boot.firm_Customize-Model 更新アプリ

boot.firm_Customize-Model を SD カードに入れ、GitHub の Releases で新しい版を探して更新する 3DS のアプリ（.cia）。
中に両方の版の boot.firm（新しい方と v9.1。どちらも英語版で、後ろに日本語のフォントを付けたもの）が入っているので、
**SD カードに boot.firm を置いてこのアプリを入れ、「最新バージョンをインストール」を選べば、boot.firm_Customize-Model がそろう。**

## できること

起動しただけでは、SD カードには何も入れない。

- **最新バージョンをインストール**: いちばん上の行。
  - GitHub に新しい版があれば、その .cia をダウンロードしてアプリを入れ直し、入れ直したアプリを起動し直す。
    入れ直したアプリが、中に入っている boot.firm を続けて入れる（`pending.txt` のしるしを見る）。
  - 新しい版が無ければ、アプリの中の boot.firm を SD カードに入れる。もう入っているときは、行の名前が「入れ直す」に変わる。
  - 入れるもの: SD:/3ds/luma/new/boot.firm（新しい方の Luma）、SD:/3ds/luma/9.1/boot.firm、
    SD:/boot.firm（いま起動している版のもの。公式の Luma3DS など、どちらの版か分からなければ新しい方）
  - 前の版が使っていた SD:/3ds/luma/13.4/ と /luma/lang.cfg は、入れ終わったら消す
- **更新内容・更新履歴**: リポジトリのいちばん上の「更新.md」を読んで出す（下の「更新.md の書き方」）。
  X で、出している版のアプリに入れ直す（前の版に戻すのも同じ）。戻せるのは、Releases に更新アプリがある v1.4 以降。
  v1.4 のアプリは、起動するとすぐに自分の boot.firm を入れる。
- **ソースコード**: ソースコードの場所（`https://github.com/<UPDATE_REPO>`）を URL と QR コードで出す。
- **更新のお知らせ**: Rosalina の起動時の自動確認を切り替える。
- **色の切り替え**: 最初の画面の Y で、黒×金と白×金を切り替える。選んだ色は `light.flag` で覚える。

画面は Rosalina と同じく、地の色に文字だけを並べる。言葉は短くし、ファイルの名前や場所、通信のエラー番号などは出さない。
言葉は日本語だけ（`source/lang.h` の `TR()` が日本語のほうを選ぶ。英語の文はソースに残してある）。

## 入れ方

1. SD カードのいちばん上に boot.firm を置いて、本体を起動する（公式の Luma3DS が入っていればそのままでよい）。
2. このアプリを入れる。
   - **SD カード**: build.bat が作る 3DS フォルダの `cia/Luma3DS_New_Updater.cia` を FBI で入れる。
   - **QR コード**: FBI（2.6.0 以降）の「Remote Install」→「Scan QR Code」で、`tools/mkqr.py` で作った QR コードを読む。
3. HOME メニューから「boot.firm_Customize-Model 更新」を起動して、「最新バージョンをインストール」を選ぶ。
   必要なファイルを入れたら、再起動するか聞く。

## 使い方

| ボタン | 最初の画面 | 更新内容 | 更新履歴 |
|---|---|---|---|
| 上下 | 行を選ぶ | 1 行ずつ送る | 版を選ぶ |
| 左右・L・R | | 1 画面ずつ送る | 上の画面の更新内容を送る |
| A | 決定 | | 選んだ版の更新内容を読む |
| B | 終わる（START でも） | 戻る | 戻る |
| X | | その版を入れる | 選んだ版を入れる |
| Y | 色を切り替える | | |

下の画面の行と、下の端の「A: …」「B: …」「X: …」「Y: …」はタッチでも選べる。

- 差し替える前のファイルは `/luma/update/backup/` に残る。
- 入れている間は HOME ボタンとスリープを止めている。

## 更新.md の書き方

リポジトリのいちばん上（既定のブランチ）に `更新.md` を置く。アプリは
`https://raw.githubusercontent.com/<UPDATE_REPO>/HEAD/更新.md` を読む。

- `## v1.5` のような見出し（`##` のあとが版の名前）ごとに 1 つの版として扱う。新しい版を上に書き足す。
- 見出しの版の後ろに書いたもの（`## v1.5 (2026-11-20)` の日付など）は、版の一覧に小さく出る。
- 最初の版の見出しより前（ファイルの題や説明）は出さない。
- 「更新内容」は、新しい版があればその版の節、無ければいちばん上の節を出す。
- 表示に反映する Markdown: 見出し（`#`〜`######`）・箇条書き（`-` `*` `+`。入れ子も）・番号付き（`1.`）・
  `**太字**`・`` `コード` ``・`[リンク](URL)`（文字だけ）・`> 引用`・区切り線（`---`）・コードの囲み（```` ``` ````）。
  `*斜体*` は飾りを外して出す。表はそのまま文字で出す。

## 起動時の自動確認（Rosalina）

Wi-Fi に繋がったあと、Rosalina が GitHub の最新のリリースの名前を 1 度だけ確かめる。
動いている版より新しければ、2 か所で知らせる。

- **Rosalina メニュー**: その回の起動で初めて開いたときに出る。
- **お知らせリスト**: 1 つの版につき 1 度だけ出る。差出人はこのアプリになる。

自動確認はこのアプリの「更新のお知らせ」で止められる（`/luma/update/off.flag` を作る）。
セーフモード（L + R + A）では確かめない。

## 新しい版を公開する手順

1. `更新.md` に新しい版の節を書き足して、GitHub に上げる。
2. `common/rosalina/include/fork.h` の `FORK_PACKAGE_TAG` を新しい版（`v1.5` など）にする
   （両方の版の Rosalina と build.bat がここから取る）。
3. build.bat の `UPDATE_REPO`（所有者/名前）を設定して `build.bat 1` でビルドする。
   `release` フォルダに、Releases に添付する `Luma3DS_New_Updater.cia` ができる。
4. GitHub でリリースを作る。
   - タグ: `FORK_PACKAGE_TAG` と同じにする。
   - 添付: `release` フォルダの `Luma3DS_New_Updater.cia`。名前を変えないこと。
   - 本文は使わない（更新内容は 更新.md から出す）。

- 「最新」は、プレリリースを除いて版（タグの数字）がいちばん新しいリリース。数字が同じ版（`1.0` と `1.0_fix` など）は、後に公開したほうを新しいとする。
- 古い版のリリースを後から作るときは、「Set as the latest release」の印を外す。
  Rosalina の起動時の確認と QR コード（`releases/latest`）は、GitHub が「Latest」にしたリリースを見るため。

## SD カードのファイル

| ファイル | 書くもの | 中身 |
|---|---|---|
| /luma/update/off.flag | このアプリ | 有ると起動時に確かめない |
| /luma/update/running.txt | Rosalina | 動いている改変版の版と、Luma の版の名前（new・9.1） |
| /luma/update/check.txt | Rosalina | 最後に確かめた日時・結果・最新の版 |
| /luma/update/notified.txt | Rosalina | お知らせリストに送った版 |
| /luma/update/tmp/ | このアプリ | ダウンロードした .cia（入れたら消す） |
| /luma/update/backup/ | このアプリ | 差し替える前のファイル |
| /luma/update/light.flag | このアプリ | 有ると画面の色を白×金にする（無ければ黒×金） |
| /luma/update/pending.txt | このアプリ | アプリを入れ直す前に書く版。入れ直したアプリが自分の版と同じなら、続けて boot.firm を入れる |

check.txt の `result` の値:

| 値 | 意味 |
|---|---|
| ok | 確かめられた |
| no_release | リリースが無い |
| http | GitHub の応答の番号（`code`）が転送でない（404 ならリポジトリ名の間違い） |
| tls | TLS の失敗（`code` は BearSSL のエラー番号） |
| dns・connect・socket | 通信の失敗 |
| memory | メモリが足りない |

## しくみと注意

- **TLS**: 3DS 本体の HTTPS（http:C）は TLS 1.1 までだが、GitHub は TLS 1.2 でないと繋がらない。
  そのため、このアプリと Rosalina は BearSSL 0.6（MIT）で TLS 1.2 を話す。
- **アプリの証明書の確認**: `source/trust_anchors.c` のルート証明書で確かめる。
  本体の日付と時刻がずれていると失敗する（TLS 54・62 など）。
- **Rosalina の確認**: 証明書は確かめず、`github.com/<リポジトリ>/releases/latest` の転送先から
  版の名前だけを取る。偽の応答で起きるのは「新しい版がある」という知らせまでで、ダウンロードはアプリが行う。
- **ダウンロードした .cia の確認**: 中身は TLS で守られている。途中で切れたものを使わないよう、
  大きさがリリースの添付ファイルと同じか確かめる。中身の正しさは、入れるときに本体（AM）が確かめる。
- **差し替え**: 新しいファイルを `<名前>.new` に書いてアプリの中のものと同じか確かめたあと、元を消して名前を変える
  （FAT は上書きの名前変更ができないため）。
  - その一瞬に電源が切れると元の名前のファイルが無くなるので、`backup/` から戻す。
  - SD:/boot.firm は最後に差し替える。
- **フォント**: boot.firm の FIRM のいちばん後ろのセクションの終わりから 0x200 バイト境界に、font_jp.bin を付けてある
  （build.bat が付ける）。boot9strap と Luma はセクションだけを読むので、起動には関わらない。
  Rosalina は SD:/boot.firm からこの位置の字を読む。
- **GitHub API の制限**: 認証なしでは 1 時間に 60 回まで（起動 1 回で 1 回使う）。

## ソース

| ファイル | 中身 |
|---|---|
| source/main.c | 動き（入れる・確かめる・更新内容・更新履歴・ソースコード・アップデート）と、画面に出す言葉 |
| source/ui.c | 画面の見た目（文字だけ。色は黒×金と白×金）とタッチの判定 |
| source/md.c | 更新.md を版ごとの節に分け、Markdown を段落に直す |
| source/screen.c | フレームバッファへの描画（四角と字）・折り返し |
| source/text.c | UTF-8 の読み取り |
| data/ui_font.bin | 画面の字（Noto Sans CJK JP。SIL Open Font License 1.1、data/LICENSE_NotoSansCJK.txt） |
| source/https.c | HTTPS の GET（BearSSL・転送・chunked） |
| source/json.c | GitHub API の応答を読む |
| source/release.c | リリースの一覧・版の比較 |
| source/local.c | SD カードのファイルの状態（アプリの中の boot.firm と比べる） |
| source/install.c | 差し替え・ダウンロード・.cia を入れる |
| source/trust_anchors.c | ルート証明書（下の手順で作る） |
| source/bearssl/ | BearSSL 0.6 のうち更新アプリだけが使う分（README.txt）。Rosalina と共通の分とヘッダーは ../common/bearssl/ |
| source/qrcodegen/ | QR コードを作る（Project Nayuki の QR Code generator library、MIT。ファイルの先頭を参照） |
| updater.rsf | makerom の設定（UniqueId 0xFF15A。Rosalina の UPDATER_TITLE_ID と合わせる。RomFS に boot.firm を入れる） |
| icon.png・icon.smdh・banner.png・banner.wav | HOME メニューのアイコンと名前（日本語と英語）、上の画面の絵（tools/mkassets.py で作る） |
| tools/mkqr.py | FBI で読む QR コードを作る |
| tools/mkfont_ui.py | data/ui_font.bin を作る |
| tools/preview.c | 画面の見本を PC で作る |

PC で試すとき（https.c・json.c・release.c・local.c・install.c・text.c・md.c は PC でもビルドできる）は、
`-DSD_ROOT=` で SD カードの代わりのフォルダを、`-DBUNDLE_ROOT=` でアプリの中の boot.firm のフォルダを、
`-DRELEASE_API_BASE=` で問い合わせ先を変えられる。

## 作り直し

- **ルート証明書（trust_anchors.c）**:
  1. Mozilla の CA 証明書の一覧（certifi の cacert.pem）を BearSSL の `brssl ta cacert.pem` で C に変える。
  2. 先頭の説明と `#include` を足す。
  3. 配列の名前を `g_trustAnchors` にして `static` を外す。
  4. `#define TAs_NUM` を `g_trustAnchorCount` の定義に変える。
- **アイコン・名前・絵**: `python3 tools/mkassets.py`（Pillow が要る）。
- **画面の字（data/ui_font.bin）**: 本文の 14 点の書体 1 つで、JIS X 0208 の字をすべて持つ。画面の言葉にそれ以外の字を
  使ったら作り直す（無い字は ? で出る）。`python3 tools/mkfont_ui.py <NotoSansCJK-Regular.ttc のあるフォルダ>`（Pillow が要る）。
- **画面の見本**: 次で作り、`./preview data/ui_font.bin 出力のフォルダ ../更新.md [light]` で PPM を書き出す（light で白×金）。
  ```
  cc -std=gnu11 -DUI_HOST -Isource -o preview tools/preview.c source/ui.c source/screen.c source/text.c source/md.c source/qrcodegen/qrcodegen.c
  ```
- **QR コード（FBI 用）**: `python3 tools/mkqr.py 所有者/名前 qr.png`（segno が要る）。
