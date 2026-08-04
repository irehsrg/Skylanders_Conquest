# Generates placeholder game SFX as 16-bit mono WAVs using only the Python
# stdlib (no numpy). These are synthesized arcade-style sounds — real audio, but
# placeholder-grade; swap them for authored clips whenever real ones exist.
#
# Run with plain python (NOT the editor):
#   python Scripts/GenerateSFX.py <output_dir>

import math
import os
import random
import struct
import sys
import wave

RATE = 44100
random.seed(7)  # deterministic output


# ---------- primitives ----------

def _osc(shape, phase):
    """One sample of a waveform at the given phase (0..1)."""
    if shape == 'sine':
        return math.sin(2.0 * math.pi * phase)
    if shape == 'square':
        return 1.0 if (phase % 1.0) < 0.5 else -1.0
    if shape == 'saw':
        return 2.0 * (phase % 1.0) - 1.0
    if shape == 'tri':
        p = phase % 1.0
        return 4.0 * p - 1.0 if p < 0.5 else 3.0 - 4.0 * p
    return 0.0


def sweep(dur, f0, f1, shape='sine', decay=3.0, gain=1.0, curve='exp'):
    """Pitch sweep from f0 to f1 with an exponential amplitude decay."""
    n = int(RATE * dur)
    out = [0.0] * n
    phase = 0.0
    for i in range(n):
        t = i / n if n else 0.0
        freq = f0 * ((f1 / f0) ** t) if curve == 'exp' and f0 > 0 else f0 + (f1 - f0) * t
        phase += freq / RATE
        env = math.exp(-decay * t)
        out[i] = _osc(shape, phase) * env * gain
    return out


def tone(dur, freq, shape='sine', decay=3.0, gain=1.0, harmonics=()):
    """Static-pitch tone, optionally with added harmonics (multiplier, level)."""
    n = int(RATE * dur)
    out = [0.0] * n
    for i in range(n):
        t = i / n if n else 0.0
        env = math.exp(-decay * t)
        ph = freq * i / RATE
        s = _osc(shape, ph)
        for mult, lvl in harmonics:
            s += _osc(shape, ph * mult) * lvl
        out[i] = s * env * gain
    return out


def noise(dur, decay=4.0, gain=1.0, lowpass=0):
    """White noise with exponential decay; lowpass = moving-average window."""
    n = int(RATE * dur)
    raw = [random.uniform(-1.0, 1.0) for _ in range(n)]
    if lowpass > 1:
        acc, smoothed = 0.0, [0.0] * n
        for i in range(n):
            acc += raw[i]
            if i >= lowpass:
                acc -= raw[i - lowpass]
                smoothed[i] = acc / lowpass
            else:
                smoothed[i] = acc / (i + 1)
        raw = smoothed
    return [raw[i] * math.exp(-decay * (i / n if n else 0)) * gain for i in range(n)]


def mix(*layers):
    """Sum layers of differing lengths."""
    n = max((len(l) for l in layers), default=0)
    out = [0.0] * n
    for layer in layers:
        for i, v in enumerate(layer):
            out[i] += v
    return out


def seq(*parts):
    """Concatenate segments back to back."""
    out = []
    for p in parts:
        out.extend(p)
    return out


def delay(dur):
    return [0.0] * int(RATE * dur)


def write_wav(samples, path):
    """Normalize, de-click the edges, and write 16-bit mono."""
    if not samples:
        return
    peak = max(abs(s) for s in samples) or 1.0
    scale = 0.89 / peak  # leave headroom

    n = len(samples)
    ramp = max(1, int(RATE * 0.003))  # 3 ms fades kill start/end clicks
    frames = bytearray()
    for i, s in enumerate(samples):
        v = s * scale
        if i < ramp:
            v *= i / ramp
        if i > n - ramp:
            v *= max(0.0, (n - i) / ramp)
        frames += struct.pack('<h', int(max(-1.0, min(1.0, v)) * 32767))

    with wave.open(path, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(bytes(frames))


# ---------- the sound set ----------

def build():
    s = {}

    # Player auto-attack: snappy laser/pop
    s['SFX_Attack_Shot'] = mix(
        sweep(0.13, 950, 260, 'square', decay=7.0, gain=0.55),
        noise(0.03, decay=30.0, gain=0.25, lowpass=3),
    )

    # Ability cast: rising magical whoosh
    s['SFX_Ability_Cast'] = mix(
        sweep(0.42, 220, 880, 'saw', decay=3.0, gain=0.40),
        sweep(0.42, 440, 1760, 'sine', decay=3.5, gain=0.28),
        noise(0.42, decay=5.0, gain=0.10, lowpass=40),
    )

    # Player death: heavy descending fall
    s['SFX_Death'] = mix(
        sweep(0.85, 420, 70, 'saw', decay=2.6, gain=0.50),
        sweep(0.85, 210, 45, 'sine', decay=2.2, gain=0.35),
    )

    # Level up: bright ascending arpeggio (C-E-G-C)
    s['SFX_LevelUp'] = seq(*[
        tone(0.11, f, 'sine', decay=6.0, gain=0.5, harmonics=((2.0, 0.35), (3.0, 0.12)))
        for f in (523.25, 659.25, 783.99, 1046.50)
    ])

    # Item purchase: two-note coin chime
    s['SFX_ItemBuy'] = seq(
        tone(0.09, 1318.5, 'sine', decay=11.0, gain=0.5, harmonics=((2.0, 0.4),)),
        tone(0.16, 1760.0, 'sine', decay=9.0, gain=0.5, harmonics=((2.0, 0.4),)),
    )

    # Tower shot: weightier zap than the player's
    s['SFX_Tower_Shot'] = mix(
        sweep(0.20, 620, 170, 'square', decay=6.0, gain=0.55),
        noise(0.06, decay=22.0, gain=0.30, lowpass=6),
    )

    # Structure destroyed: explosion (rumble + debris)
    s['SFX_Structure_Destroy'] = mix(
        noise(1.0, decay=3.6, gain=0.60, lowpass=18),
        sweep(1.0, 130, 38, 'sine', decay=2.8, gain=0.55),
        noise(0.10, decay=26.0, gain=0.30, lowpass=2),
    )

    # Phoenix respawn: triumphant rising chord
    s['SFX_Phoenix_Respawn'] = mix(
        sweep(0.95, 262, 523, 'sine', decay=1.7, gain=0.38),
        sweep(0.95, 330, 659, 'sine', decay=1.7, gain=0.30),
        sweep(0.95, 392, 784, 'sine', decay=1.7, gain=0.24),
    )

    # Titan attack: deep artillery thump
    s['SFX_Titan_Attack'] = mix(
        sweep(0.30, 180, 52, 'sine', decay=5.0, gain=0.65),
        noise(0.07, decay=20.0, gain=0.28, lowpass=10),
    )

    # Jungle camp cleared: mid-weight thud
    s['SFX_Camp_Death'] = mix(
        sweep(0.45, 300, 90, 'tri', decay=4.5, gain=0.50),
        noise(0.20, decay=9.0, gain=0.22, lowpass=14),
    )

    return s


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else 'SFX'
    os.makedirs(out_dir, exist_ok=True)
    sounds = build()
    for name, samples in sounds.items():
        path = os.path.join(out_dir, name + '.wav')
        write_wav(samples, path)
        print('wrote {}  ({:.2f}s)'.format(path, len(samples) / RATE))
    print('{} files -> {}'.format(len(sounds), out_dir))


if __name__ == '__main__':
    main()
