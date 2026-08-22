#!/usr/bin/env python3
"""Fallback spectrum source for Omadrop.

Captures the default sink's PipeWire monitor and prints one frame per line in
cava's raw ascii format (`v;v;...;v;`, values 0..1000). cava is the preferred
backend; this exists so the plugin has something to draw before anyone
installs it, and so the QML side only ever parses one format.
"""

import argparse
import os
import subprocess
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from analysis import HPSS, BeatTracker

RATE = 44100
HOP = 512          # samples consumed per emitted frame (~86 fps at 44.1k)
WINDOW = 2048      # samples the FFT sees; overlaps so low bins stay resolved
RANGE = 1000       # matches cava's ascii_max_range
FMIN, FMAX = 30.0, 16000.0
GATE = 2e-4        # below this peak amplitude, treat the stream as silence
WAVE_N = 128       # waveform points sent per frame


def default_sink():
    """PipeWire node name of the current default sink, if it can be read."""
    try:
        return subprocess.run(
            ["pactl", "get-default-sink"],
            capture_output=True, text=True, timeout=2,
        ).stdout.strip() or None
    except Exception:
        return None


def band_edges(bars):
    """Log-spaced FFT bin boundaries, so bass does not eat every bar."""
    edges = np.geomspace(FMIN, FMAX, bars + 1) / RATE * WINDOW
    return np.clip(edges.astype(int), 1, WINDOW // 2 - 1)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bars", type=int, default=64)
    ap.add_argument("--framerate", type=int, default=60)
    args = ap.parse_args()
    bars = max(4, args.bars)

    # Capturing what a sink is playing is `stream.capture.sink=true` against
    # the sink node itself. The familiar `<sink>.monitor` name is PulseAudio
    # nomenclature -- pw-record accepts it as a target, links to nothing, and
    # hands back a stream of digital silence with no error.
    cmd = ["pw-record", "--raw", "--rate", str(RATE), "--channels", "1",
           "--format", "f32", "--latency", "20ms",
           "-P", "{ stream.capture.sink=true }"]
    sink = default_sink()
    if sink:
        cmd += ["--target", sink]
    cmd += ["-"]

    rec = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)

    edges = band_edges(bars)
    window = np.hanning(WINDOW).astype(np.float32)
    fps = RATE / HOP
    hpss = HPSS(bars)
    beat = BeatTracker(fps)
    prev_norm = np.zeros(bars, dtype=np.float32)
    perc_ceil = harm_ceil = 1e-3

    # MilkDrop-style audio normalisation. Every audio variable is a RATIO
    # against a ~4.2s running loudness, not an absolute level -- nominal 1.0
    # whatever the track's mastering, <0.7 quiet, >1.3 loud, spiking to 3-5 on
    # hits. Peak-normalising into 0..1 (what this used to do) saturates and
    # throws away exactly the dynamics that read as punch.
    # Rates are MilkDrop's 30fps constants adjusted to our frame rate.
    _r_rise = 0.2 ** (30.0 / fps)
    _r_fall = 0.5 ** (30.0 / fps)
    _r_long = 0.992 ** (30.0 / fps)
    imm_avg = np.ones(3, dtype=np.float32) * 1e-3
    long_avg = np.ones(3, dtype=np.float32) * 1e-3
    # Music puts almost all its energy below 1 kHz. Without a tilt the top two
    # thirds of the display never move.
    tilt = np.geomspace(1.0, 12.0, bars).astype(np.float32)
    buf = np.zeros(WINDOW, dtype=np.float32)
    smooth = np.zeros(bars, dtype=np.float32)
    ceiling = 1e-3
    need = HOP * 4

    while True:
        raw = rec.stdout.read(need)
        if not raw or len(raw) < need:
            break
        buf = np.roll(buf, -HOP)
        buf[-HOP:] = np.frombuffer(raw, dtype=np.float32)

        if float(np.abs(buf).max()) < GATE:
            smooth *= 0.80
        else:
            mag = np.abs(np.fft.rfft(buf * window))
            vals = np.array(
                [mag[edges[i]:max(edges[i] + 1, edges[i + 1])].max() for i in range(bars)],
                dtype=np.float32,
            ) * tilt

            # Auto-gain against a slowly decaying ceiling: loud tracks stop
            # clipping, quiet ones still fill the screen.
            ceiling = max(float(vals.max()), ceiling * 0.9995, 1e-3)
            norm = np.clip(vals / ceiling, 0.0, 1.0)

            # Asymmetric: snap up on a transient, sag down slowly, so a kick
            # reads as a hit instead of a blur.
            smooth = np.where(norm > smooth,
                              smooth * 0.30 + norm * 0.70,
                              smooth * 0.82 + norm * 0.18)

            # --- musical analysis ---------------------------------------
            # Percussive vs harmonic, so drums and sustained tones can drive
            # separate visual channels instead of competing for one.
            hpss.push(norm)
            perc_v, harm_v = hpss.split()
            perc_ceil = max(float(perc_v.sum()), perc_ceil * 0.9995, 1e-3)
            harm_ceil = max(float(harm_v.sum()), harm_ceil * 0.9995, 1e-3)
            perc = min(1.0, float(perc_v.sum()) / perc_ceil)
            harm = min(1.0, float(harm_v.sum()) / harm_ceil)

            # Three bands from the raw (pre-auto-gain) magnitudes, so the
            # long-term average is meaningful.
            nb = len(vals)
            imm = np.array([
                float(vals[: int(nb * 0.18)].sum()),
                float(vals[int(nb * 0.18): int(nb * 0.55)].sum()),
                float(vals[int(nb * 0.55):].sum()),
            ], dtype=np.float32)
            rise = imm > imm_avg
            imm_avg = np.where(rise,
                               imm_avg * _r_rise + imm * (1 - _r_rise),
                               imm_avg * _r_fall + imm * (1 - _r_fall))
            long_avg = long_avg * _r_long + imm * (1 - _r_long)
            rel = np.clip(imm / np.maximum(long_avg, 1e-6), 0.0, 6.0)
            att = np.clip(imm_avg / np.maximum(long_avg, 1e-6), 0.0, 6.0)

            # Beat tracking is driven by percussive flux -- feeding it the
            # full mix lets sustained notes smear the onset envelope.
            flux = float(np.maximum(0.0, norm - prev_norm).sum())
            prev_norm = norm.copy()
            beat.push(flux)
            beat.advance()

            # Time-domain waveform. MilkDrop draws the oscilloscope trace and
            # it is a large part of why it reads as synced -- it IS the audio,
            # not a derived statistic. Downsampled to WAVE_N points, encoded
            # 0..1000 with 500 as zero crossing.
            seg = buf[-WAVE_N * 8:]
            wav = seg.reshape(WAVE_N, -1).mean(axis=1)
            wpk = max(float(np.abs(wav).max()), 1e-4)
            wav = np.clip(wav / (wpk * 1.15), -1.0, 1.0)
            sys.stdout.write(
                "^" + ";".join(str(int(v * 499 + 500)) for v in wav) + "\n"
            )

            sys.stdout.write(
                "~%.2f;%.4f;%.3f;%.4f;%.4f;%.4f;%.4f;%.3f;%.3f;%.3f;%.3f;%.3f;%.3f\n"
                % (beat.bpm, beat.phase, beat.conf, perc, harm,
                   beat.bar_phase, beat.to_next_beat,
                   rel[0], rel[1], rel[2], att[0], att[1], att[2])
            )

        sys.stdout.write(";".join(str(int(v * RANGE)) for v in smooth) + ";\n")
        sys.stdout.flush()

    rec.terminate()


if __name__ == "__main__":
    try:
        main()
    except (BrokenPipeError, KeyboardInterrupt):
        pass
