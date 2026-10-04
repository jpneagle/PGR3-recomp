"""contact.py <frames dir> <out.png> [cols] - tile frames into one image with timestamps."""
import os, sys
from PIL import Image, ImageDraw
d, out = sys.argv[1], sys.argv[2]; cols = int(sys.argv[3]) if len(sys.argv) > 3 else 6
files = sorted(f for f in os.listdir(d) if f.endswith('.bmp'))
tw, th = 320, 180
rows = (len(files) + cols - 1) // cols
sheet = Image.new('RGB', (cols * tw, rows * th), (40, 40, 40))
dr = ImageDraw.Draw(sheet)
for i, f in enumerate(files):
    im = Image.open(os.path.join(d, f)).convert('RGB'); im.thumbnail((tw, th))
    x, y = i % cols * tw, i // cols * th
    sheet.paste(im, (x, y)); dr.text((x + 4, y + 4), f[:-4], fill=(255, 255, 0))
sheet.save(out); print(len(files), 'frames', Image.open(os.path.join(d, files[0])).size if files else '')
