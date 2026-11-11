#!/bin/sh
# Generates src/atari_http_server/ui/index.html (C array of the gzipped
# web interface) - replaces the SCons rule, no xxd needed.
set -e
VERSION=${1:-382}
UI=src/atari_http_server/ui
for p in $UI/img/*.png; do
  { printf '"'; base64 -w 0 < "$p"; echo '"'; } > "${p%.png}.dat"
done
cpp -E -P -CC -undef -nostdinc -DVERSION=$VERSION -I"$UI" \
    -DUSE_LESSC=0 -DUSE_UGLIFYJS=0 $UI/webinterface.html > /tmp/index.html.cpp
gzip -9 -n -c /tmp/index.html.cpp > /tmp/index.html.gz
python3 - /tmp/index.html.gz $UI/index.html <<'EOF'
import sys
d = open(sys.argv[1], 'rb').read()
with open(sys.argv[2], 'w') as f:
    f.write('static unsigned char index_html_gz[] = {\n')
    for i in range(0, len(d), 12):
        f.write('  ' + ', '.join('0x%02x' % b for b in d[i:i+12]) + ',\n')
    f.write('};\nstatic unsigned int index_html_gz_len = %d;\n' % len(d))
EOF
echo "UI: $(wc -c < /tmp/index.html.gz) bytes gz"
