"""Generate placeholder game SFX (16-bit mono 44.1 kHz WAV) with only the stdlib.

Writes to <project>/Saved/SFXStaging/. Deterministic (seeded RNG).
Run:  python Scripts/gen_placeholder_sfx.py
"""
import math
import os
import random
import struct
import wave

SR = 44100
TWO_PI = 2.0 * math.pi
PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(PROJECT_DIR, "Saved", "SFXStaging")

rng = random.Random(1337)


# ---------------------------------------------------------------- primitives
def n_samples(dur):
    return int(round(dur * SR))


def silence(dur):
    return [0.0] * n_samples(dur)


def mix(dst, src, gain=1.0, offset=0.0):
    """Add src into dst starting at offset seconds (extends dst if needed)."""
    start = int(offset * SR)
    end = start + len(src)
    if end > len(dst):
        dst.extend([0.0] * (end - len(dst)))
    for i, v in enumerate(src):
        dst[start + i] += v * gain
    return dst


def exp_env(n, decay_s, attack_s=0.0):
    """Exponential decay envelope; decay_s = time constant in seconds."""
    a = max(1, int(attack_s * SR))
    out = []
    for i in range(n):
        e = math.exp(-(i / SR) / decay_s)
        if attack_s > 0 and i < a:
            e *= i / a
        out.append(e)
    return out


def noise(dur, decay_s, attack_s=0.0, color=0.0):
    """White noise with exp decay. color in [0,1): one-pole lowpass amount."""
    n = n_samples(dur)
    env = exp_env(n, decay_s, attack_s)
    out, prev = [], 0.0
    for i in range(n):
        w = rng.uniform(-1.0, 1.0)
        prev = color * prev + (1.0 - color) * w
        out.append(prev * env[i])
    return out


def highpass(sig, amount=0.9):
    """Simple one-pole highpass (brightens noise)."""
    out, px, py = [], 0.0, 0.0
    for x in sig:
        y = amount * (py + x - px)
        out.append(y)
        px, py = x, y
    return out


def sweep(dur, f0, f1, decay_s, shape="sine", curve="exp", attack_s=0.0, harmonics=None):
    """Pitched sweep from f0 to f1 Hz. shape: sine | square | saw."""
    n = n_samples(dur)
    env = exp_env(n, decay_s, attack_s)
    out, phase = [], 0.0
    for i in range(n):
        p = i / max(1, n - 1)
        if curve == "exp":
            f = f0 * (f1 / f0) ** p
        else:
            f = f0 + (f1 - f0) * p
        phase += TWO_PI * f / SR
        if shape == "square":
            # band-limited-ish square: first few odd harmonics
            v = (math.sin(phase) + math.sin(3 * phase) / 3 + math.sin(5 * phase) / 5
                 + math.sin(7 * phase) / 7) * 0.9
        elif shape == "saw":
            v = sum(math.sin(k * phase) / k for k in range(1, 7)) * 0.6
        else:
            v = math.sin(phase)
            if harmonics:
                for k, g in harmonics:
                    v += g * math.sin(k * phase)
        out.append(v * env[i])
    return out


def tone(dur, freq, decay_s, attack_s=0.0, harmonics=None, vibrato=0.0, vib_rate=6.0):
    n = n_samples(dur)
    env = exp_env(n, decay_s, attack_s)
    out, phase = [], 0.0
    for i in range(n):
        f = freq * (1.0 + vibrato * math.sin(TWO_PI * vib_rate * i / SR))
        phase += TWO_PI * f / SR
        v = math.sin(phase)
        if harmonics:
            for k, g in harmonics:
                v += g * math.sin(k * phase)
        out.append(v * env[i])
    return out


def thump(dur, f_start, f_end, decay_s):
    """Low sine kick: fast pitch drop + exp decay."""
    n = n_samples(dur)
    out, phase = [], 0.0
    for i in range(n):
        t = i / SR
        f = f_end + (f_start - f_end) * math.exp(-t / 0.025)
        phase += TWO_PI * f / SR
        out.append(math.sin(phase) * math.exp(-t / decay_s))
    return out


def swell_env(sig, rise_s, fall_s):
    r = max(1, int(rise_s * SR))
    out = []
    for i, v in enumerate(sig):
        if i < r:
            e = (i / r) ** 2
        else:
            e = math.exp(-((i - r) / SR) / fall_s)
        out.append(v * e)
    return out


def soft_clip(sig, drive=1.5):
    k = math.tanh(drive)
    return [math.tanh(v * drive) / k for v in sig]


def finalize(sig, dur, fade_in_s=0.004, fade_out_s=0.012, peak_db=-3.0):
    """Trim/pad to exact length, DC-remove, fade in/out, peak-normalise."""
    n = n_samples(dur)
    sig = (sig + [0.0] * n)[:n]
    mean = sum(sig) / n
    sig = [v - mean for v in sig]
    fi = max(1, int(fade_in_s * SR))
    fo = max(1, int(fade_out_s * SR))
    for i in range(min(fi, n)):
        sig[i] *= i / fi
    for i in range(min(fo, n)):
        sig[n - 1 - i] *= i / fo
    peak = max(abs(v) for v in sig) or 1.0
    g = (10 ** (peak_db / 20.0)) / peak
    return [v * g for v in sig]


def write_wav(name, sig):
    path = os.path.join(OUT_DIR, name)
    frames = bytearray()
    for v in sig:
        frames += struct.pack("<h", int(max(-1.0, min(1.0, v)) * 32767))
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(bytes(frames))
    return path


# ------------------------------------------------------------------- sounds
NOTE_C5, NOTE_E5, NOTE_G5, NOTE_C6 = 523.25, 659.25, 783.99, 1046.50
BELL = [(2, 0.35), (3, 0.15), (4, 0.08)]


def hit_light():
    d = 0.12
    s = silence(d)
    mix(s, highpass(noise(0.05, 0.008), 0.85), 0.9)
    mix(s, sweep(d, 2000, 600, 0.03), 0.6)
    mix(s, noise(d, 0.02, color=0.5), 0.3)
    return finalize(soft_clip(s, 1.8), d)


def hit_heavy():
    d = 0.35
    s = silence(d)
    mix(s, thump(d, 160, 90, 0.11), 1.0)
    mix(s, noise(0.15, 0.03, color=0.45), 0.7)
    mix(s, highpass(noise(0.03, 0.006), 0.8), 0.5)
    mix(s, sweep(0.12, 900, 250, 0.03), 0.25)
    return finalize(soft_clip(s, 2.0), d)


def ability_cast():
    d = 0.4
    s = silence(d)
    mix(s, swell_env(sweep(0.3, 300, 1400, 10.0, shape="saw"), 0.22, 0.04), 0.6)
    mix(s, swell_env(sweep(0.3, 600, 2800, 10.0), 0.22, 0.04), 0.15)
    mix(s, noise(0.25, 0.08, color=0.7), 0.35, offset=0.15)
    mix(s, swell_env(noise(0.3, 10.0, color=0.3), 0.25, 0.05), 0.12)
    return finalize(soft_clip(s, 1.4), d)


def ability_ult():
    d = 1.1
    s = silence(d)
    mix(s, thump(d, 120, 42, 0.28), 1.0)
    mix(s, sweep(0.8, 1600, 120, 0.3, shape="saw"), 0.45)
    mix(s, sweep(0.7, 2400, 300, 0.2), 0.2)
    mix(s, noise(d, 0.35, attack_s=0.01, color=0.75), 0.8)
    mix(s, highpass(noise(0.08, 0.015), 0.8), 0.5)
    return finalize(soft_clip(s, 2.2), d, fade_out_s=0.05)


def death():
    d = 0.8
    s = silence(d)
    mix(s, soft_clip(sweep(0.7, 500, 80, 0.35, shape="saw"), 3.5), 0.7)
    mix(s, sweep(0.7, 1000, 160, 0.25), 0.15)
    mix(s, noise(d, 0.25, attack_s=0.05, color=0.8), 0.5)
    mix(s, thump(0.3, 140, 60, 0.08), 0.5)
    return finalize(soft_clip(s, 1.8), d, fade_out_s=0.05)


def levelup():
    d = 0.9
    s = silence(d)
    for i, f in enumerate((NOTE_C5, NOTE_E5, NOTE_G5)):
        t0 = i * 0.11
        mix(s, tone(d - t0, f, 0.25 if i < 2 else 0.4, attack_s=0.004, harmonics=BELL), 0.5, t0)
    # shimmer: high partials with fast tremolo
    sh = []
    for k in range(n_samples(0.6)):
        t = k / SR
        v = (math.sin(TWO_PI * NOTE_C6 * 2 * t) + math.sin(TWO_PI * NOTE_G5 * 3.01 * t)) * 0.5
        v *= 0.5 + 0.5 * math.sin(TWO_PI * 14 * t)
        sh.append(v * math.exp(-t / 0.2) * min(1.0, t / 0.03))
    mix(s, sh, 0.15, 0.22)
    mix(s, highpass(noise(0.5, 0.15, attack_s=0.05), 0.95), 0.06, 0.22)
    return finalize(soft_clip(s, 1.2), d, fade_out_s=0.05)


def buy_item():
    d = 0.25
    s = silence(d)
    coin = [(2.76, 0.3), (5.4, 0.1)]
    mix(s, tone(0.2, 2600, 0.035, harmonics=coin), 0.6, 0.0)
    mix(s, tone(0.19, 3400, 0.05, harmonics=coin), 0.6, 0.06)
    mix(s, highpass(noise(0.01, 0.002), 0.9), 0.2, 0.0)
    mix(s, highpass(noise(0.01, 0.002), 0.9), 0.2, 0.06)
    return finalize(soft_clip(s, 1.2), d)


def kill():
    d = 0.6
    s = silence(d)
    mix(s, thump(0.3, 150, 70, 0.07), 0.9)
    mix(s, noise(0.12, 0.025, color=0.4), 0.6)
    mix(s, highpass(noise(0.02, 0.004), 0.8), 0.4)
    mix(s, sweep(0.45, 880, 1320, 0.15, attack_s=0.005, harmonics=BELL), 0.55, 0.09)
    mix(s, tone(0.4, 1760, 0.12, attack_s=0.005, harmonics=[(2, 0.2)]), 0.25, 0.16)
    return finalize(soft_clip(s, 1.6), d, fade_out_s=0.04)


def tower_shot():
    d = 0.3
    s = silence(d)
    zap = sweep(0.25, 1800, 400, 0.08, shape="square")
    zap = [v * (0.7 + 0.3 * math.sin(TWO_PI * 90 * i / SR)) for i, v in enumerate(zap)]
    mix(s, zap, 0.6)
    mix(s, highpass(noise(0.2, 0.04), 0.7), 0.35)
    mix(s, thump(0.15, 200, 90, 0.04), 0.35)
    return finalize(soft_clip(s, 2.0), d)


def respawn():
    d = 0.7
    s = silence(d)
    for i, f in enumerate((261.63, 329.63, 392.0, 523.25)):
        t = tone(d, f, 10.0, harmonics=[(2, 0.2), (3, 0.05)], vibrato=0.003, vib_rate=5 + i)
        mix(s, swell_env(t, 0.45, 0.12), 0.3)
    mix(s, swell_env(sweep(d, 400, 1600, 10.0), 0.5, 0.08), 0.08)
    mix(s, swell_env(noise(d, 10.0, color=0.6), 0.45, 0.1), 0.15)
    return finalize(soft_clip(s, 1.1), d, fade_out_s=0.08)


def ui_click():
    d = 0.06
    s = silence(d)
    mix(s, tone(0.04, 1800, 0.006), 0.6)
    mix(s, highpass(noise(0.01, 0.0015), 0.85), 0.5)
    mix(s, tone(0.04, 600, 0.008), 0.3)
    return finalize(s, d, fade_in_s=0.003, fade_out_s=0.005)


def recall():
    d = 1.0
    s = silence(d)
    for f, g in ((659.25, 0.3), (987.77, 0.25), (1318.5, 0.2), (1975.5, 0.12)):
        t = sweep(d, f * 0.8, f * 1.25, 10.0, harmonics=[(2, 0.1)])
        t = [v * (0.6 + 0.4 * math.sin(TWO_PI * 9 * i / SR + f)) for i, v in enumerate(t)]
        mix(s, swell_env(t, 0.65, 0.15), g)
    mix(s, swell_env(highpass(noise(d, 10.0), 0.95), 0.6, 0.15), 0.12)
    return finalize(soft_clip(s, 1.1), d, fade_out_s=0.1)


SOUNDS = [
    ("hit_light.wav", 0.12, hit_light),
    ("hit_heavy.wav", 0.35, hit_heavy),
    ("ability_cast.wav", 0.4, ability_cast),
    ("ability_ult.wav", 1.1, ability_ult),
    ("death.wav", 0.8, death),
    ("levelup.wav", 0.9, levelup),
    ("buy_item.wav", 0.25, buy_item),
    ("kill.wav", 0.6, kill),
    ("tower_shot.wav", 0.3, tower_shot),
    ("respawn.wav", 0.7, respawn),
    ("ui_click.wav", 0.06, ui_click),
    ("recall.wav", 1.0, recall),
]


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    print("%-18s %7s %7s %8s %7s" % ("file", "target", "actual", "samples", "peakdB"))
    ok = True
    for name, dur, fn in SOUNDS:
        path = write_wav(name, fn())
        with wave.open(path, "rb") as w:
            frames = w.getnframes()
            assert w.getnchannels() == 1 and w.getsampwidth() == 2 and w.getframerate() == SR
            data = w.readframes(frames)
        actual = frames / SR
        peak = max(abs(v) for v in struct.unpack("<%dh" % frames, data)) / 32767.0
        good = abs(actual - dur) < 0.001
        ok = ok and good
        print("%-18s %7.3f %7.3f %8d %7.2f%s" % (name, dur, actual, frames,
              20 * math.log10(peak), "" if good else "  MISMATCH"))
    print("Wrote %d files to %s" % (len(SOUNDS), OUT_DIR))
    if not ok:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
