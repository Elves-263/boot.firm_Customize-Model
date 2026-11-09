# ビルド手順と今回の変更点

## ビルド

```bash
export DEVKITPRO=/opt/devkitpro
export DEVKITARM=$DEVKITPRO/devkitARM
./build.sh
```

v9.1 → v13.4 の順に続けてビルドします。**v9.1 が失敗したら v13.4 には進みません**
（`make` の終了コードだけでなく `boot.firm` の実体も確認します）。
どちらの段で失敗したかは最後に表示されます。

| 環境変数 | 効果 |
|---|---|
| `ONLY=v13.4` | v13.4 だけビルド。v9.1 を直したくないときの再試行用 |
| `ONLY=v9.1` | v9.1 だけビルド |
| `KEEP_BUILD=1` | `make clean` を省いて差分ビルド |
| `STRICT=1` | 全警告をエラーにして棚卸し |
| `K11_MAX=0x10000` | k11_extension が指定サイズを超えたら失敗させる |
| `V134_CTRU=v2.6.2` | v13.4 の libctru バージョンを変更 |

必要なもの: devkitARM, armips, makerom, firmtool, git, make。

`firmtool` が MSYS2 から見えない場合、build.sh が Windows 側の Python の Scripts
フォルダなどを自動で探して PATH に足します。恒久的に通しておきたいときは:

```bash
echo 'export PATH="$PATH:/c/Users/<ユーザー名>/AppData/Local/Programs/Python/Python314/Scripts"' >> ~/.bashrc
```
libctru は `.libctru/` に v2.2.2 (v9.1 用) と v2.3.1 (v13.4 用) を自動で用意します。

出来上がったら `dist/` の中身をそのまま SD カードのルートへ:

```
SD:/boot.firm                  ← 既定で起動する版（初期状態は v13.4）
SD:/3ds/luma/13.4/boot.firm
SD:/3ds/luma/9.1/boot.firm
```

`SD:/luma/chainload_target` は起動時に自動生成されるので手で作る必要はありません。

## 変更点

### 1. ビルドフラグ（O3DS クラッシュの最有力候補）

全 Makefile から `-fpermissive` を削除し、`-Wno-error` をやめました。

GCC 14 以降は C でも `-fpermissive` が効いてしまい、**暗黙の関数宣言・非互換ポインタ代入・
int/ポインタ変換**をエラーから警告に格下げします。そこへ `-Wno-error` が重なると、
本来ビルドを止めるはずの不具合がすべて素通りします。この種のバグは
「片方の機種では動くがもう片方で落ちる」という出方をするので、まずここを潰してください。

代わりに全 Makefile へ、的を絞った設定を入れてあります:

```make
HARDERRORS := -Werror=implicit-function-declaration -Werror=implicit-int \
              -Werror=return-type \
              -Wno-error=int-conversion -Wno-error=incompatible-pointer-types
```

GCC 14 以降はこれらが全部「既定でエラー」になりました。`-fpermissive` はそれをまとめて
警告に格下げしてしまうので使いません。代わりに二分します。

- **エラーのまま維持** — `implicit-function-declaration`（戻り値が int と仮定され、
  u64・float・構造体を返す関数で破綻する）、`implicit-int`、`return-type`（未定義の値が返る）。
  実行時に本当に壊れるのはこの系統です。
- **警告に格下げ** — `int-conversion`、`incompatible-pointer-types`。
  ARM32 ではポインタも整数も 32bit で同じレジスタに乗るため生成コードは変わらず、
  2018 年の v9.1 ツリーには元から大量にあるので、止めるとビルドが進みません。
  ただし `build.log` に残り、ビルド終了時に一覧が出るので目視で確認できます。

一度だけ全警告をエラーにして棚卸ししたい場合は `STRICT=1 ./build.sh`。

`HARDERRORS` に引っかかってビルドが止まったら、**それが探していたバグです**。
`-fpermissive` を戻して黙らせるのではなく、コード側を直してください。

### 1b. v9.1 draw.c の型不一致を修正

上の設定を入れた最初のビルドで `sysmodules/rosalina/source/draw.c:247` が引っかかりました。

```c
svcFlushProcessDataCache(CUR_PROCESS_HANDLE, FB_BOTTOM_VRAM_ADDR, FB_BOTTOM_SIZE);
```

`FB_BOTTOM_VRAM_ADDR` は `((void *)0x1F48F000)` ですが、libctru v2.2.2 の
`svcFlushProcessDataCache()` は第2引数が `u32` です。上流 v13.4 と同じく `(u32)` を付けました。
これは v9.1 に元からあるもので、ARM32 では実害はありません
（`-fpermissive` が無いと GCC 14 では止まる、というだけです）。

なお v9.1 の `k11_extension/Makefile` の `-fcommon` は残しました。リンカスクリプトが
`*(.bss* COMMON)` を拾うので動作は正しく、GCC 10+ では必要です。

#
### 1c. v13.4 の libctru を v2.7.0 に変更

v13.4 の `sysmodules/rosalina/source/main.c` と `menu.c` は `qtmGetSessionHandle()` を使いますが、
これは libctru **v2.4.0** で追加された API です。元の build.sh が指定していた v2.3.1 には存在せず、
暗黙の宣言 → その戻り値を `*` で参照 → コンパイルエラーになります。

上流 README は「libctru と Luma3DS のリリースは同期している」としているので、
v13.4（2026-04）の直前のリリースである **v2.7.0**（2025-12-31）を既定にしました。
環境変数で上書きできます:

```bash
V134_CTRU=v2.6.2 ./build.sh
```

v9.1 側は v2.2.2 のままで問題なくビルドできています。

### 1d. C 専用の診断を C++ に渡さない

v13.4 の loader は C++ を含むため、`-Werror=implicit-function-declaration` などが
`cc1plus: warning: ... is not valid for C++` を大量に出していました。
`HARDERRORS` を `CFLAGS` のみに付け、`CXXFLAGS` には C++ でも有効な `-Werror=return-type` だけを残しました。

## 2. k11_extension のサイズ監視（O3DS 固有）

`arm9/source/patches.c`:

```c
u32 dstKextPA = (ISN3DS ? 0x2E000000 : 0x26C00000) - stolenSystemMemRegionSize;
// no need to steal any more mem on N3DS. Currently, everything fits in BASE on O3DS too (?)
```

Old 3DS では k11 拡張を BASE メモリ領域から削り取って置きます。上流のコメント自体が
「O3DS でも今のところ収まってる(?)」と不確かで、拡張が膨らむと **O3DS だけ落ちます**
（N3DS は余裕があるので気付けません）。サイズは `__end__ - ベースVA` なので .bss の増加も効きます。

`build.sh` が毎回このサイズを表示します。無改造 + 純正ツールチェーンでビルドした値と比べて
明らかに大きければ、それが原因です。上限を決めて自動で弾きたい場合:

```bash
K11_MAX=0x10000 ./build.sh
```

### 3. v13.4: チェインロード失敗を握りつぶさない

v9.1 に 2026-07-11 で入れた修正が v13.4 に反映されていなかったので移植しました。
`arm9/source/main.c`:

```c
if(!loadVersionFirm() && isSdMode)
    error("Version switch failed: target boot.firm is missing or corrupted.");
```

これまでは失敗しても黙って自分自身を起動していたため、
「クラッシュした」のか「切替に失敗して元の版が立ち上がっただけ」なのか区別できませんでした。

### 4. 両ツリー: 切替先 boot.firm を FIRM マジックで検証

`sysmodules/rosalina/source/menus.c` の `chainloadVersionFirmExists()` が
サイズしか見ていなかった件（コード中の TODO）を修正し、先頭 4 バイトの `FIRM` も確認します。
壊れた firm を掴んだまま再起動してしまう事故を防げます。

### 5. その他

- `Luma3DS_v13_4/Makefile`: git 管理外で `git describe` がエラーを吐くのを抑制。
- `build.sh` をツリー構成に合わせて修正し、`dist/` に SD 配置をそのまま作るようにしました。

## SD カードが壊れている可能性について

`config.ini` が同じフォルダに複数できる、`chainload_targettitles` のように
2つのファイル名が繋がった名前のファイルができる——これらは**正常な FAT では起こりません**。
ディレクトリエントリの破損です。

原因として一番ありそうなのは、**チェインロードで固まった状態からの電源長押し**です。
Rosalina が SD に書いた直後に再起動し、その後 ARM9 の FatFs が同じカードを触るため、
途中で電源を切ると FAT の整合性が崩れます。それを繰り返すと壊れが溜まります。

### 対処

1. SD をカードリーダーに挿し、まず**バックアップを取る**
2. Windows なら `chkdsk X: /f`、Linux/macOS なら `fsck.vfat -a /dev/sdX1` で検査・修復
3. 直らなければ **SD をフォーマットし直して入れ直す**（32GB 以下は FAT32、64GB 以上は
   exFAT ではなく FAT32 にすること。Luma / boot9strap は exFAT を扱えません）

### 書き込みを減らす対策（実装済み）

以前は ARM9 が**毎起動 `chainload_target` を書いて**いました。書く必要のない場面でも
毎回 SD に書くため、破損の機会が増えます。これを廃止し、状態ファイルを
`/luma/chainload_ver.flag` の 1 本だけにしました。

- Rosalina は自分のバージョンをコンパイル時に知っているので、現在版を SD から読む必要がない
- 書き込みは「切り替えを実行した時」の 1 回だけ
- ARM9 はそのファイルを読んで消し、目標が自分自身なら何もしない


## 配布するとき

ライセンス上の義務は `LICENSING.md` にまとめてあります。特に:

- Luma3DS は **GPLv3**。バイナリを配るならソースも一緒に公開すること
- Luma3DS の追加条項（GPLv3 第7条 b/c）により、**本家と混同されないよう明示**する義務がある


## まだ原因が特定できない場合

`SD:/luma/dumps/arm9` または `arm11` に例外ダンプが残ります。
`Luma3DS_v9_1/exceptions/exception_dump_parser/` で解析すれば発生箇所が分かります。
そのとき以下も併せて分かると切り分けが早いです:

- どちらの版が落ちるか (v9.1 / v13.4)
- 落ちるタイミング（電源投入直後 / Rosalina を開いた時 / バージョン切替時）
- O3DS 側の本体更新バージョン
