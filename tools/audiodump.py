"""audiodump.py <dump.raw> [out.wav] - analyse an --audio_dump_file capture
(frames of 6 planar channels x 256 big-endian float samples, 48 kHz)."""
import sys, wave
import numpy as np
a = np.fromfile(sys.argv[1], dtype='>f4')
n = len(a) // 1536
a = a[:n * 1536].reshape(n, 6, 256)
ch = a.transpose(1, 0, 2).reshape(6, -1)           # channel, samples
secs = ch.shape[1] / 48000
print(f"{n} frames, {secs:.1f} s")
names = ['FL', 'FR', 'FC', 'LFE', 'BL', 'BR']
step = 48000 * 5
for c in range(6):
    rms = [float(np.sqrt(np.mean(ch[c, i:i + step] ** 2))) for i in range(0, ch.shape[1], step)]
    print(names[c], ' '.join(f"{20*np.log10(r+1e-9):4.0f}" for r in rms))
# frames that are entirely silent on the front pair, per 5 s
front = np.abs(a[:, 0:2, :]).max(axis=(1, 2))
per = 5 * 48000 // 256
print('silent frames /5s:', ' '.join(str(int((front[i:i + per] == 0).sum())) for i in range(0, n, per)))
print('peak', float(np.abs(ch).max()))
if len(sys.argv) > 2:
    st = np.clip(ch[0:2].T, -1, 1)
    w = wave.open(sys.argv[2], 'wb'); w.setnchannels(2); w.setsampwidth(2); w.setframerate(48000)
    w.writeframes((st * 32767).astype('<i2').tobytes()); w.close()
