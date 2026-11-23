#!/usr/bin/env python3
"""Copy C sources without comments, indentation or blank lines, to fit the
App Store's 1 MB for an app's files. The code is unchanged: string and
character literals are kept byte for byte, and the build checks that every
game draws the same frames from the stripped core as from vendor/.

  strip_c.py <src> <dst> [<licence file to put on top>]
"""
import sys

def strip(src):
    out, i, n = [], 0, len(src)
    while i < n:
        c = src[i]
        if c in '"\'':
            j = i + 1
            while j < n and src[j] != c:
                j += 2 if src[j] == '\\' else 1
            out.append(src[i:j + 1])
            i = j + 1
        elif src.startswith('//', i):
            j = i
            # a // comment ends at a newline that isn't escaped
            while j < n and src[j] != '\n':
                j += 2 if src[j] == '\\' else 1
            i = j
        elif src.startswith('/*', i):
            j = src.find('*/', i + 2)
            if j < 0:
                raise SystemExit('unterminated comment')
            out.append(' ')
            i = j + 2
        else:
            out.append(c)
            i += 1
    lines = []
    for line in ''.join(out).split('\n'):
        s = line.strip()
        if s:
            lines.append(s)
    return '\n'.join(lines) + '\n'

def main():
    src = open(sys.argv[1], encoding='utf-8').read()
    body = strip(src)
    head = ''
    if len(sys.argv) > 3:
        head = '/*\n' + open(sys.argv[3], encoding='utf-8').read().strip() + '\n\nComments and indentation removed by Pocket Arcade (tools/strip_c.py)\nto fit the App Store\'s size limit; the original is in vendor/.\n*/\n'
    open(sys.argv[2], 'w', encoding='utf-8').write(head + body)

if __name__ == '__main__':
    main()
