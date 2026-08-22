"""Musical analysis for ASCIIscope.

Two pieces the three-band spectrum cannot provide:

  HPSS        splits the spectrum into percussive (drums, transients) and
              harmonic (vocals, pads, sustained tones) so they can drive
              separate visual channels instead of fighting over one.

  BeatTracker estimates tempo and beat phase from the onset envelope, which
              lets the visuals *anticipate* a beat rather than react ~80ms
              after it, and lets scene changes land on a bar rather than on
              whichever onset happens to fire next.

Both are cheap enough to run per frame at ~86fps. Real source separation
(Demucs and friends) is neural, wants its own GPU and has latency measured in
seconds -- useless here. HPSS gets the part that matters for visuals.
"""

import numpy as np

__all__ = ["HPSS", "BeatTracker"]


def _median_filter_1d(x, k):
    """Median filter along a 1-D array, edge-padded."""
    if k <= 1:
        return x
    pad = k // 2
    padded = np.pad(x, pad, mode="edge")
    windows = np.lib.stride_tricks.sliding_window_view(padded, k)
    return np.median(windows, axis=-1).astype(np.float32)


class HPSS:
    """Harmonic/percussive split by median filtering the spectrogram.

    Sustained tones are stable over time, so a median across time isolates
    them. Percussive hits are broadband, so a median across frequency isolates
    those. Soft (Wiener) masks then split the current frame between the two.
    """

    def __init__(self, bins, t_len=17, f_len=17):
        self.t_len = t_len
        self.f_len = f_len
        self.buf = np.zeros((t_len, bins), dtype=np.float32)

    def push(self, mag):
        self.buf = np.roll(self.buf, -1, axis=0)
        self.buf[-1] = mag

    def split(self):
        cur = self.buf[-1]
        harm = np.median(self.buf, axis=0).astype(np.float32)
        perc = _median_filter_1d(cur, self.f_len)
        h2 = harm * harm
        p2 = perc * perc
        denom = h2 + p2 + 1e-9
        mask_h = h2 / denom
        return cur * (1.0 - mask_h), cur * mask_h


class BeatTracker:
    """Tempo and beat phase from an onset-strength envelope.

    Tempo comes from the autocorrelation of the envelope, weighted by a
    log-normal prior around 120 BPM -- without that prior the peak lands on
    half or double time about as often as on the truth. Phase comes from
    correlating a pulse train against the recent envelope, and is then run
    through a phase accumulator so it stays smooth between estimates.
    """

    def __init__(self, fps, history_s=8.0, bpm_min=62.0, bpm_max=190.0):
        self.fps = float(fps)
        self.n = int(history_s * fps)
        self.env = np.zeros(self.n, dtype=np.float32)
        self.min_lag = max(2, int(self.fps * 60.0 / bpm_max))
        self.max_lag = min(self.n - 1, int(self.fps * 60.0 / bpm_min))

        lags = np.arange(self.n, dtype=np.float32)
        with np.errstate(divide="ignore", invalid="ignore"):
            bpms = 60.0 * self.fps / np.maximum(lags, 1.0)
        # Octave errors are the classic failure; bias toward human tempo.
        self.prior = np.exp(-0.5 * (np.log2(np.maximum(bpms, 1.0) / 120.0) / 0.85) ** 2)
        self.prior[: self.min_lag] = 0.0
        self.prior[self.max_lag + 1 :] = 0.0

        self.period = self.fps * 60.0 / 120.0
        self.bpm = 120.0
        self.conf = 0.0
        self.phase = 0.0      # 0..1 within the current beat
        self.beat_index = 0
        self._since_estimate = 0

    def push(self, flux):
        self.env = np.roll(self.env, -1)
        self.env[-1] = float(flux)

    def _estimate(self):
        e = self.env - self.env.mean()
        if not np.any(e):
            return
        # Autocorrelation via FFT.
        spec = np.fft.rfft(e, 2 * self.n)
        ac = np.fft.irfft(spec * np.conj(spec))[: self.n].astype(np.float32)
        if ac[0] > 0:
            ac /= ac[0]
        scored = ac * self.prior
        lag = int(np.argmax(scored))
        if lag < self.min_lag:
            return

        # Parabolic interpolation for sub-frame tempo resolution.
        if 0 < lag < self.n - 1:
            y0, y1, y2 = scored[lag - 1], scored[lag], scored[lag + 1]
            denom = y0 - 2 * y1 + y2
            if abs(denom) > 1e-9:
                lag = lag + 0.5 * (y0 - y2) / denom

        self.period = float(np.clip(lag, self.min_lag, self.max_lag))
        self.bpm = 60.0 * self.fps / self.period
        self.conf = float(np.clip(scored[int(round(self.period))] * 3.0, 0.0, 1.0))

        # Phase: which offset lines a pulse train up with the envelope best.
        span = int(min(self.n, self.period * 4))
        seg = self.env[-span:]
        p = int(round(self.period))
        if p < 2:
            return
        best_off, best_score = 0, -1.0
        for off in range(p):
            idx = np.arange(off, span, p)
            score = float(seg[idx].sum())
            if score > best_score:
                best_score, best_off = score, off
        # Frames since the most recent pulse.
        since = (span - 1 - best_off) % p
        measured = (since / self.period) % 1.0
        # Nudge rather than jump, so phase stays smooth.
        delta = (measured - self.phase + 0.5) % 1.0 - 0.5
        self.phase = (self.phase + delta * 0.35) % 1.0

    def advance(self):
        """Call once per frame after push(). Returns True on a beat crossing."""
        self._since_estimate += 1
        if self._since_estimate >= 16:
            self._since_estimate = 0
            self._estimate()

        prev = self.phase
        self.phase = (self.phase + 1.0 / max(self.period, 1e-6)) % 1.0
        crossed = self.phase < prev
        if crossed:
            self.beat_index += 1
        return crossed

    @property
    def bar_phase(self):
        """Position within a 4/4 bar, 0..1."""
        return ((self.beat_index % 4) + self.phase) / 4.0

    @property
    def to_next_beat(self):
        """Seconds until the next beat."""
        return (1.0 - self.phase) * self.period / self.fps
