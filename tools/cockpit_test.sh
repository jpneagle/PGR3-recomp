#!/bin/sh
# tools/cockpit_test.sh <name> [cvar args...] - drive to the London race, switch to the in-car view
# (LB twice) and report the mean brightness of the last frames (black screen = ~0).
cd "$(dirname "$0")/.."
name=$1; shift
S=$(sed 's/,rt@112000-140000//' tools/scripts/race.txt)
S="$S,lb@120000-120200,lb@123000-123200"
PGR3_AUTOPRESS="$S" PGR3_DUMP_FRAMES=2000 PGR3_DUMP_DIR=titles/pgr3/run/c_$name \
  tools/run.sh c_$name 132 --user_language=2 --fullscreen=false "$@" | head -1
python - "$name" <<'PY'
import os, sys
from PIL import Image, ImageStat
d = 'titles/pgr3/run/c_' + sys.argv[1]; fs = sorted(os.listdir(d))[-3:]
for f in fs:
    im = Image.open(d + '/' + f).convert('L'); w, h = im.size
    print(f, im.size, 'center mean %.1f' % ImageStat.Stat(im.crop((w // 4, h // 4, 3 * w // 4, 3 * h // 4))).mean[0])
im = Image.open(d + '/' + fs[-1]); im.thumbnail((900, 600)); im.save('titles/pgr3/run/c_' + sys.argv[1] + '.png')
PY
