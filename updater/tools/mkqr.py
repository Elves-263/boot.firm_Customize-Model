#!/usr/bin/env python3
# FBI（2.6.0 以降）の「Remote Install → Scan QR Code」で読む QR コードを作る。
#
#   pip install segno
#   python3 tools/mkqr.py 所有者/名前 [出力.png] [タグ]
#
# タグを書かなければ、最新のリリースの .cia を指す URL にする
#   https://github.com/<所有者/名前>/releases/latest/download/Luma3DS_New_Updater.cia
# GitHub はこれを最新のリリースの添付ファイルへ転送するので、版を上げても同じ QR コードのままでよい。
# 転送をたどれない FBI のために、タグ（v1.5 など）を書くとその版の .cia を直接指す URL にする。

import sys

import segno

ASSET = 'Luma3DS_New_Updater.cia'


def main():
    if len(sys.argv) < 2 or '/' not in sys.argv[1]:
        sys.exit('使い方: python3 tools/mkqr.py 所有者/名前 [出力.png] [タグ]')

    repo = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else 'qr.png'
    if len(sys.argv) > 3:
        url = 'https://github.com/%s/releases/download/%s/%s' % (repo, sys.argv[3], ASSET)
    else:
        url = 'https://github.com/%s/releases/latest/download/%s' % (repo, ASSET)

    segno.make(url, error='m').save(out, scale=8, border=4)
    print(url)


if __name__ == '__main__':
    main()
