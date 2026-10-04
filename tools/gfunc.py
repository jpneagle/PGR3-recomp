"""gfunc.py <sub_XXXXXXXX> [stats] - print a recompiled function's guest instructions (from the
generated C++ comments), or with 'stats' its mnemonic histogram and call targets."""
import sys, glob, re, collections
name = sys.argv[1]
for f in glob.glob('generated/default/pgr3_recomp.*.cpp'):
    s = open(f, encoding='utf-8', errors='replace').read()
    i = s.find('REX_FUNC(%s)' % name)
    if i < 0: continue
    j = s.find('\n}\n', i)
    ins = [l[4:] for l in s[i:j].split('\n') if l.startswith('\t// ')]
    if len(sys.argv) > 2:
        c = collections.Counter(x.split()[0] for x in ins)
        print(len(ins), 'instructions:', ' '.join(f'{k}:{v}' for k, v in c.most_common()))
        print('calls:', ' '.join(sorted({x.split()[1] for x in ins if x.startswith('bl ')})))
    else:
        print('\n'.join(ins))
    break
