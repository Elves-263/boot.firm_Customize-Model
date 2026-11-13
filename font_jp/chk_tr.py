# TR("日本語", "英語") の組で、書式指定（%d など）の並びが同じか確かめる。GCC が検査するのは、使うほうの言語だけなので
#
#   python3 font_jp/chk_tr.py Luma3DS_new/sysmodules/rosalina/source Luma3DS_v9_1/sysmodules/rosalina/source \
#                             common/rosalina/source Luma3DS_new/arm9/source Luma3DS_v9_1/source updater/source
import re, os, sys
SPEC = re.compile(r'%[-+ #0]*(\*|\d+)?(\.(\*|\d+))?(hh|h|ll|l|z|j|t)?[diouxXcspfFeEgGaA%]')
LIT = r'"(?:[^"\\\n]|\\.)*"'
TRX = re.compile(r'(?<![A-Za-z0-9_])TR\(\s*((?:' + LIT + r'|\s|[A-Z_][A-Z0-9_]*)+?)\s*,\s*((?:' + LIT + r'|\s|[A-Z_][A-Z0-9_]*)+?)\s*\)', re.S)
def text(group):
    return ''.join(re.findall(r'"((?:[^"\\\n]|\\.)*)"', group))
bad = n = 0
for root in sys.argv[1:]:
    for dp, dn, fn in os.walk(root):
        if 'bearssl' in dp: continue
        for f in fn:
            if not f.endswith('.c'): continue
            p = os.path.join(dp, f)
            s = open(p, encoding='utf-8').read()
            for m in TRX.finditer(s):
                n += 1
                ja, en = text(m.group(1)), text(m.group(2))
                sj = [x.group(0).replace('-', '') for x in SPEC.finditer(ja)]
                se = [x.group(0).replace('-', '') for x in SPEC.finditer(en)]
                norm = lambda l: [re.sub(r'\d+', '', x) for x in l]
                if norm(sj) != norm(se):
                    bad += 1
                    print('%s:%d: %r vs %r' % (p, s[:m.start()].count('\n') + 1, sj, se))
print('TR pairs:', n, 'mismatch:', bad)
