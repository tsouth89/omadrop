import QtQuick
import Quickshell
import Quickshell.Io
import Quickshell.Wayland
import Quickshell.Services.Mpris

// Omadrop -- a braille-grid music visualiser.
//
// Everything visible is drawn by one fragment shader. The CPU's per-frame job
// is updating a handful of audio uniforms; the GPU evaluates the field at each
// cell's eight braille dot positions, packs them into a glyph index, and
// samples the real font glyph out of an atlas. That keeps the terminal
// aesthetic while running entirely on the GPU.
ShellRoot {
  id: root

  // Follow the system monospace (fontconfig) so Omadrop matches whatever font
  // the user's theme sets. OMADROP_FONT overrides. The face must carry braille
  // (U+2800-28FF); the doctor checks that.
  property string fontFamily: Quickshell.env("OMADROP_FONT") || "monospace"
  // Derived from this file's own location so the app is relocatable --
  // /usr/share, ~/.local/share, a git checkout, all work unchanged.
  readonly property string appDir: {
    var u = Qt.resolvedUrl(".").toString()
    if (u.indexOf("file://") === 0) u = u.substring(7)
    return u.replace(/\/$/, "")
  }

  // ------------------------------------------------------- cover art
  // Whichever player is actually playing wins; otherwise fall back to any
  // player so a paused track still shows its cover.
  readonly property var activePlayer: {
    var best = null
    var list = Mpris.players ? Mpris.players.values : []
    for (var i = 0; i < list.length; i++) {
      var p = list[i]
      if (!p) continue
      if (p.isPlaying) return p
      if (!best) best = p
    }
    return best
  }

  readonly property string artUrl: activePlayer && activePlayer.trackArtUrl
    ? String(activePlayer.trackArtUrl) : ""
  readonly property string trackTitle: activePlayer ? String(activePlayer.trackTitle || "") : ""
  readonly property string trackArtist: activePlayer ? String(activePlayer.trackArtist || "") : ""

  // Two covers so a track change can crossfade between them.
  property string artPathA: ""
  property string artPathB: ""
  property bool showingB: false
  property real artFade: 0

  onArtUrlChanged: root.fetchArt(root.artUrl)
  Component.onCompleted: {
    root.fetchArt(root.artUrl)
    // Dev: OMADROP_SCENE=<0-3> jumps straight to one fully-devolved scene.
    var f = parseInt(Quickshell.env("OMADROP_SCENE") || "-1")
    if (f >= 0) {
      root.sequenceRunning = false
      root.manualDissolve = 1
      root.sceneA = f
      root.sceneB = f
      root.sceneBlend = 0
      if (Quickshell.env("OMADROP_BEAT") === "1") root.forceBeat = true
      console.log("FORCEBEAT env=" + Quickshell.env("OMADROP_BEAT") + " forceBeat=" + root.forceBeat)
      console.log("FORCED scene=" + f + " sequenceRunning=" + root.sequenceRunning
                  + " manualDissolve=" + root.manualDissolve)
    }
  }

  function fetchArt(url) {
    if (!url || url.length === 0) return
    artFetch.running = false
    artFetch.command = [root.appDir + "/bin/art-fetch", url]
    artFetch.running = true
  }

  // Load into whichever slot is currently hidden, then fade to it.
  function setArt(path) {
    if (path.length === 0) return
    if (root.showingB) {
      if (root.artPathA === path) return
      root.artPathA = path
    } else {
      if (root.artPathB === path) return
      root.artPathB = path
    }
  }

  // There is one Image pair per screen, so every monitor reports the same
  // cover as ready. Reveal once per path or the toggles cancel out.
  property string revealedPath: ""
  readonly property bool hasArt: root.artPathA.length > 0 || root.artPathB.length > 0

  function revealArt(path) {
    if (path.length === 0 || root.revealedPath === path) return
    root.revealedPath = path
    root.phaseT = 0
    // Every track begins in Genesis. The first abstract world is responsible
    // for carrying the cover's own structure into motion.
    root.sceneA = 1
    root.sceneB = 4
    root.sceneBlend = 0
    root.sceneT = 0
    root.changeArmed = false
    root.showingB = !root.showingB
    fadeAnim.to = root.showingB ? 1 : 0
    fadeAnim.restart()
  }

  NumberAnimation {
    id: fadeAnim
    target: root
    property: "artFade"
    duration: 1400
    easing.type: Easing.InOutCubic
  }

  Process {
    id: artFetch
    stdout: StdioCollector {
      onStreamFinished: {
        var p = String(text).trim()
        if (p.length) root.setArt(p)
      }
    }
    stderr: SplitParser {
      splitMarker: "\n"
      onRead: function(l) { if (l.length) console.log("art:", l) }
    }
  }

  // Audio, smoothed on the QML side so the shader gets continuous values.
  property real bass: 0
  property real mid: 0
  property real treble: 0
  property real energy: 0
  property real onset: 0

  Process {
    id: source
    running: true
    command: [root.appDir + "/bin/omadrop-source", "128", "60"]
    stdout: SplitParser {
      splitMarker: "\n"
      onRead: function(line) {
        // Time-domain waveform.
        if (line.charAt(0) === "^") {
          var w = line.substring(1).split(";")
          if (w.length < 32) return
          var out = new Array(w.length)
          for (var wi = 0; wi < w.length; wi++)
            out[wi] = Math.max(0, Math.min(1, (parseInt(w[wi], 10) || 500) / 1000))
          root.wave = out
          return
        }
        // Feature lines from the analysis engine.
        if (line.charAt(0) === "~") {
          var f = line.substring(1).split(";")
          if (f.length < 7) return
          root.bpm = parseFloat(f[0]) || 0
          root.beatPhase = parseFloat(f[1]) || 0
          root.beatConf = parseFloat(f[2]) || 0
          root.perc = parseFloat(f[3]) || 0
          root.harm = parseFloat(f[4]) || 0
          root.barPhase = parseFloat(f[5]) || 0
          root.toNextBeat = parseFloat(f[6]) || 0
          if (f.length >= 13) {
            root.relBass = parseFloat(f[7]) || 1;  root.relMid = parseFloat(f[8]) || 1
            root.relTreb = parseFloat(f[9]) || 1;  root.attBass = parseFloat(f[10]) || 1
            root.attMid = parseFloat(f[11]) || 1;  root.attTreb = parseFloat(f[12]) || 1
          }
          return
        }
        var p = line.split(";")
        var n = p.length - 1
        if (n < 16) return

        var loN = Math.floor(n * 0.18), mdN = Math.floor(n * 0.55)
        var lo = 0, md = 0, hi = 0, flux = 0
        var prev = root.prevBins
        var cur = new Array(n)

        for (var i = 0; i < n; i++) {
          var v = parseInt(p[i], 10) / 1000
          if (!isFinite(v)) v = 0
          cur[i] = v
          // Spectral flux -- positive change only. Compressed music holds a
          // near-constant level, so absolute loudness barely moves; the
          // attacks are what actually carry the rhythm.
          if (prev && prev.length === n) { var d = v - prev[i]; if (d > 0) flux += d }
          if (i < loN) lo = Math.max(lo, v)
          else if (i < mdN) md = Math.max(md, v)
          else hi = Math.max(hi, v)
        }
        root.prevBins = cur

        // Normalise each band against its own slowly-decaying running peak so
        // a quiet passage still drives the full range.
        root.loPk = Math.max(lo, root.loPk * 0.9990, 0.05)
        root.mdPk = Math.max(md, root.mdPk * 0.9990, 0.05)
        root.hiPk = Math.max(hi, root.hiPk * 0.9990, 0.05)
        root.fxPk = Math.max(flux, root.fxPk * 0.9985, 0.02)

        var bn = Math.min(1, lo / root.loPk)
        var mn = Math.min(1, md / root.mdPk)
        var hn = Math.min(1, hi / root.hiPk)
        var fn = Math.min(1, flux / root.fxPk)

        root.bassS = bn > root.bassS ? root.bassS * 0.55 + bn * 0.45 : root.bassS * 0.86 + bn * 0.14
        root.midS = mn > root.midS ? root.midS * 0.55 + mn * 0.45 : root.midS * 0.88 + mn * 0.12
        root.trebleS = hn > root.trebleS ? root.trebleS * 0.60 + hn * 0.40 : root.trebleS * 0.89 + hn * 0.11
        root.bass = root.bass * 0.72 + root.bassS * 0.28
        root.mid = root.mid * 0.74 + root.midS * 0.26
        root.treble = root.treble * 0.76 + root.trebleS * 0.24
        root.energy = root.energy * 0.90 + ((bn + mn + hn) / 3) * 0.10

        // A transient is flux well above its recent average, not a fixed
        // threshold -- that adapts across tracks and genres.
        // ---- 32 bands for spatial mapping -----------------------------
        // Three band maxima cannot carry counterpoint: everything ends up
        // driven by the same number and the whole frame thumps as one. These
        // let each scene map frequency onto position.
        var NS = 32
        if (!root.specPk) {
          root.specPk = new Array(NS); root.specLvl = new Array(NS)
          for (var z = 0; z < NS; z++) { root.specPk[z] = 0.15; root.specLvl[z] = 0 }
        }
        var sp = new Array(NS)
        for (var q = 0; q < NS; q++) {
          var q0 = Math.floor(q * n / NS), q1 = Math.max(q0 + 1, Math.floor((q + 1) * n / NS))
          var mx = 0
          for (var qq = q0; qq < q1; qq++) if (cur[qq] > mx) mx = cur[qq]
          root.specPk[q] = Math.max(mx, root.specPk[q] * 0.9988, 0.06)
          var nv = Math.min(1, mx / root.specPk[q])

          // Per-band transient. A steady level barely moves once it is
          // peak-normalised, so level alone makes bass legible (kicks are huge
          // transients) and everything above it invisible. Each band gets its
          // own attack signal.
          var prevL = root.specLvl[q]
          root.specLvl[q] = nv
          var tr = Math.max(0, nv - prevL) * 4.0

          // High bands decay faster than low ones -- a hi-hat should tick, a
          // bass note should swell.
          var rel = 0.945 - 0.13 * (q / (NS - 1))
          var old = root.spec ? root.spec[q] : 0
          var target = Math.min(1, nv * 0.45 + tr * 0.95)
          sp[q] = target > old ? old * 0.45 + target * 0.55 : old * rel + target * (1 - rel)
        }
        root.spec = sp

        // ---- structural novelty -------------------------------------
        // Aggregate to 16 log-compressed bands: per-bin shape is far too
        // noisy to tell a section change from a cymbal.
        var NB = 16, bands = new Array(NB)
        for (var b = 0; b < NB; b++) {
          var s0 = Math.floor(b * n / NB), s1 = Math.max(s0 + 1, Math.floor((b + 1) * n / NB))
          var acc = 0
          for (var k = s0; k < s1; k++) acc += cur[k]
          bands[b] = Math.log(1 + (acc / (s1 - s0)) * 12)
        }
        if (!root.novShort) { root.novShort = bands.slice(); root.novLong = bands.slice() }
        var dotp = 0, ns = 0, nl = 0
        for (b = 0; b < NB; b++) {
          root.novShort[b] += 0.026 * (bands[b] - root.novShort[b])   // ~0.45s
          root.novLong[b]  += 0.0023 * (bands[b] - root.novLong[b])   // ~5s
          dotp += root.novShort[b] * root.novLong[b]
          ns += root.novShort[b] * root.novShort[b]
          nl += root.novLong[b] * root.novLong[b]
        }
        var raw = 1 - dotp / (Math.sqrt(ns) * Math.sqrt(nl) + 1e-9)
        // A section change stays different; a transient does not.
        root.novelty += 0.015 * (raw - root.novelty)
        root.novBase += 0.0012 * (root.novelty - root.novBase)

        root.fluxAvg = root.fluxAvg * 0.92 + fn * 0.08
        // A refractory period is what separates beat-tracking from jitter:
        // without it this fires ~14x/sec on micro-transients. 10 frames at
        // ~86fps lands around 3-4/sec, which is beats plus some subdivision.
        // Bass beat. A raw per-frame delta threshold does NOT work: measured
        // on real material the p95 frame-to-frame bass delta is 0.051 against
        // a 0.10 threshold, so the old test fired 0.14x/sec -- once every
        // seven seconds. Flux above its own running average, same principle
        // as the onset detector, lands at ~1.4/sec (beat level).
        root.bassAvg += 0.05 * (bn - root.bassAvg)
        var bfl = Math.max(0, bn - root.bassAvg)
        root.bassFluxAvg += 0.02 * (bfl - root.bassFluxAvg)
        if (root.pulseCool > 0) root.pulseCool--
        else if (bfl > root.bassFluxAvg * 1.5 && bfl > 0.04) { root.pulse = 1; root.pulseCool = 24 }

        if (root.onsetCool > 0) root.onsetCool--
        else if (fn > root.fluxAvg * 1.5 && fn > 0.18) {
          root.onset = 1
          root.onsetCool = 10
        }
      }
    }
    stderr: SplitParser {
      splitMarker: "\n"
      onRead: function(l) { if (l.length) console.warn("source:", l) }
    }
  }
  property var prevBins: null
  property real loPk: 0.2
  property real mdPk: 0.2
  property real hiPk: 0.2
  property real fxPk: 0.1
  property real fluxAvg: 0
  property int onsetCool: 0
  property real pulse: 0
  property bool forceBeat: false
  property bool freeze: Quickshell.env("OMADROP_FREEZE") === "1"
  property real freezeTime: parseFloat(Quickshell.env("OMADROP_TIME") || "30") || 30
  property int pulseCool: 0
  property real bassAvg: 0
  property real bassFluxAvg: 0
  property real slowEnergy: 0
  property real bassS: 0
  property real midS: 0
  property real trebleS: 0
  property var spec: null
  property var wave: null
  // Pre-built styles: Qt.rgba() per sample would dominate the paint loop.
  readonly property var waveStyles: {
    var a = []
    for (var i = 0; i < 256; i++) a.push(Qt.rgba(i / 255, 0, 0, 1))
    return a
  }

  // ------------------------------------------------ musical features
  property real bpm: 0
  property real beatPhase: 0
  property real beatConf: 0
  property real perc: 0
  property real harm: 0
  property real barPhase: 0
  property real toNextBeat: 0
  // Ratios against a ~4.2s running loudness -- nominal 1.0 regardless of how
  // the track is mastered. Deviation from 1 is the musical signal.
  property real relBass: 1
  property real relMid: 1
  property real relTreb: 1
  property real attBass: 1
  property real attMid: 1
  property real attTreb: 1

  // With exact beat phase these need no detection at all -- they are pure
  // functions of where we are in the beat, so they can never fire late.
  // impact peaks ON the beat and decays through it; swell builds INTO the
  // next one, which is what lets the visuals anticipate rather than react.
  readonly property real beatImpact: Math.exp(-root.beatPhase * 2.6) * root.beatConf
  readonly property real beatSwell: (root.beatPhase < 0.55 ? 0
      : (root.beatPhase - 0.55) / 0.45) * root.beatConf
  property var specPk: null
  property var specLvl: null
  property var novShort: null
  property var novLong: null
  property real novelty: 0
  property real novBase: 0.02

  Variants {
    model: Quickshell.screens

    PanelWindow {
      id: win
      required property var modelData
      screen: modelData

      anchors { top: true; bottom: true; left: true; right: true }
      exclusionMode: ExclusionMode.Ignore
      WlrLayershell.namespace: "omadrop"
      WlrLayershell.layer: WlrLayer.Overlay
      WlrLayershell.keyboardFocus: WlrKeyboardFocus.Exclusive
      color: "#000000"

      // Cell geometry from the real font metrics, so atlas glyphs are not
      // stretched and the grid reads like a terminal.
      property int targetRows: root.rowsTarget
      readonly property int fontPixelSize: Math.max(6, Math.round(height / targetRows / 1.18))
      readonly property real cellH: Math.max(1, fm.height)
      readonly property real cellW: Math.max(1, fm.advanceWidth("⣿"))
      readonly property int rows: Math.max(8, Math.floor(height / cellH))
      readonly property int cols: Math.max(8, Math.floor(width / cellW))

      FontMetrics {
        id: fm
        font.family: root.fontFamily
        font.pixelSize: win.fontPixelSize
      }

      // ------------------------------------------------- 256-glyph atlas
      readonly property int atlasCols: 16
      // Match the atlas glyph to its on-screen size. Minifying a large atlas
      // blurs the dots together; this keeps them crisp, exactly like a
      // terminal rendering the glyph at that point size.
      readonly property real dpr: (screen && screen.devicePixelRatio) ? screen.devicePixelRatio : 1
      readonly property int atlasCellH: Math.max(10, Math.round(cellH * dpr))
      readonly property int atlasCellW: Math.max(6, Math.round(cellW * dpr))

      Grid {
        id: atlasGrid
        columns: win.atlasCols
        spacing: 0
        Repeater {
          model: 256
          Text {
            required property int index
            width: win.atlasCellW
            height: win.atlasCellH
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            color: "white"
            font.family: root.fontFamily
            font.pixelSize: Math.round(win.atlasCellH * 0.84)
            text: String.fromCharCode(0x2800 + index)
          }
        }
      }

      ShaderEffectSource {
        id: atlasTexture
        sourceItem: atlasGrid
        hideSource: true
        live: false
        smooth: true
        textureSize: Qt.size(win.atlasCols * win.atlasCellW, 16 * win.atlasCellH)
      }

      Image {
        id: artImageA
        source: root.artPathA.length ? "file://" + root.artPathA : ""
        visible: false
        smooth: true
        cache: true
        onStatusChanged: if (status === Image.Ready) root.revealArt(root.artPathA)
      }
      Image {
        id: artImageB
        source: root.artPathB.length ? "file://" + root.artPathB : ""
        visible: false
        smooth: true
        cache: true
        onStatusChanged: if (status === Image.Ready) root.revealArt(root.artPathB)
      }

      readonly property vector4d uGrid: Qt.vector4d(win.cols, win.rows, win.atlasCols, 16)
      readonly property vector4d uAudio: Qt.vector4d(root.bass, root.mid, root.treble, root.energy)
      readonly property vector4d uAnim: Qt.vector4d(clock.t, root.onset, root.dissolve, root.artFade)
        // aspect, artScale (1.0 = cover fills the full height), gain, vignette
        // aspect, zoom (1.0 = cover bleeds to full width), gain, edge amount
      readonly property vector4d uView: Qt.vector4d(width / Math.max(1, height), root.zoom, root.gain, root.edge)
        // Monochrome covers get tinted toward the theme accent instead of
        // rendering as grey dots.
      readonly property vector4d uTint: Qt.vector4d(root.tintColor.r, root.tintColor.g,
                                            root.tintColor.b, root.tintAmount)
      readonly property vector4d uFx: Qt.vector4d(root.vignette, root.hasArt ? 1 : 0, root.musicalTime, root.forceBeat ? 1.0 : root.pulse)
      readonly property vector4d uScene: Qt.vector4d(root.sceneA, root.sceneB, root.sceneBlend, root.sceneT)
      readonly property vector4d uSp0: root.spec ? Qt.vector4d(root.spec[0], root.spec[1], root.spec[2], root.spec[3]) : Qt.vector4d(0,0,0,0)
      readonly property vector4d uSp1: root.spec ? Qt.vector4d(root.spec[4], root.spec[5], root.spec[6], root.spec[7]) : Qt.vector4d(0,0,0,0)
      readonly property vector4d uSp2: root.spec ? Qt.vector4d(root.spec[8], root.spec[9], root.spec[10], root.spec[11]) : Qt.vector4d(0,0,0,0)
      readonly property vector4d uSp3: root.spec ? Qt.vector4d(root.spec[12], root.spec[13], root.spec[14], root.spec[15]) : Qt.vector4d(0,0,0,0)
      readonly property vector4d uSp4: root.spec ? Qt.vector4d(root.spec[16], root.spec[17], root.spec[18], root.spec[19]) : Qt.vector4d(0,0,0,0)
      readonly property vector4d uSp5: root.spec ? Qt.vector4d(root.spec[20], root.spec[21], root.spec[22], root.spec[23]) : Qt.vector4d(0,0,0,0)
      readonly property vector4d uSp6: root.spec ? Qt.vector4d(root.spec[24], root.spec[25], root.spec[26], root.spec[27]) : Qt.vector4d(0,0,0,0)
      readonly property vector4d uSp7: root.spec ? Qt.vector4d(root.spec[28], root.spec[29], root.spec[30], root.spec[31]) : Qt.vector4d(0,0,0,0)
      readonly property vector4d uMusic: Qt.vector4d(root.beatPhase, root.beatImpact,
                                             root.beatSwell, root.beatConf)
      readonly property vector4d uRel: Qt.vector4d(root.relBass, root.relMid, root.relTreb, 0)
      readonly property vector4d uAtt: Qt.vector4d(root.attBass, root.attMid, root.attTreb, 0)
      readonly property vector4d uMusic2: Qt.vector4d(root.perc, root.harm,
                                              root.barPhase, root.slowEnergy)
      readonly property vector4d uPal0: Qt.vector4d(root.palColor(0).r, root.palColor(0).g, root.palColor(0).b, 1)
      readonly property vector4d uPal1: Qt.vector4d(root.palColor(1).r, root.palColor(1).g, root.palColor(1).b, 1)
      readonly property vector4d uPal2: Qt.vector4d(root.palColor(2).r, root.palColor(2).g, root.palColor(2).b, 1)
      readonly property vector4d uPal3: Qt.vector4d(root.palColor(3).r, root.palColor(3).g, root.palColor(3).b, 1)

      // Waveform texture. fillRect only -- putImageData silently does nothing
      // in this Qt build (see NOTES.md).
      Canvas {
        id: waveCanvas
        width: 128; height: 1
        renderTarget: Canvas.Image
        renderStrategy: Canvas.Immediate
        // The parser lives at root scope and there is one canvas per screen,
        // so the repaint is driven from here rather than called directly.
        Connections {
          target: root
          function onWaveChanged() { waveCanvas.requestPaint() }
        }
        onPaint: {
          var w = root.wave
          if (!w) return
          var ctx = getContext("2d")
          var n = Math.min(w.length, 128)
          for (var i = 0; i < n; i++) {
            ctx.fillStyle = root.waveStyles[Math.round(w[i] * 255)]
            ctx.fillRect(i, 0, 1, 1)
          }
        }
      }
      ShaderEffectSource {
        id: waveTexture
        sourceItem: waveCanvas
        live: true
        smooth: true
        hideSource: false
        textureSize: Qt.size(128, 1)
      }

      // ------------------------------------------------- feedback chain
      // The field pass renders into an accumulator that samples ITSELF each
      // frame through a slow warp and decays. That single mechanism is what
      // produces trails, tunnels and drifting structure -- the things the eye
      // can actually follow. Rendered at exactly one texel per braille dot.
      ShaderEffect {
        id: fieldPass
        width: Math.max(8, win.cols * 2)
        height: Math.max(8, win.rows * 4)
        blending: false
        vertexShader: Qt.resolvedUrl("shaders/braille.vert.qsb")
        fragmentShader: Qt.resolvedUrl("shaders/field.frag.qsb")
        property variant artTex: artImageA
        property variant artTexB: artImageB
        property variant prevTex: accum
        property variant waveTex: waveTexture
        property vector4d grid: win.uGrid
        property vector4d audio: win.uAudio
        property vector4d anim: win.uAnim
        property vector4d view: win.uView
        property vector4d tint: win.uTint
        property vector4d fx: win.uFx
        property vector4d scene: win.uScene
        property vector4d sp0: win.uSp0
        property vector4d sp1: win.uSp1
        property vector4d sp2: win.uSp2
        property vector4d sp3: win.uSp3
        property vector4d sp4: win.uSp4
        property vector4d sp5: win.uSp5
        property vector4d sp6: win.uSp6
        property vector4d sp7: win.uSp7
        property vector4d music: win.uMusic
        property vector4d music2: win.uMusic2
        property vector4d rel: win.uRel
        property vector4d att: win.uAtt
        property vector4d pal0: win.uPal0
        property vector4d pal1: win.uPal1
        property vector4d pal2: win.uPal2
        property vector4d pal3: win.uPal3
      }

      ShaderEffectSource {
        id: accum
        sourceItem: fieldPass
        recursive: true
        live: true
        wrapMode: ShaderEffectSource.Repeat
        hideSource: true
        smooth: true
        textureSize: Qt.size(Math.max(8, win.cols * 2), Math.max(8, win.rows * 4))
      }

      // Display pass: braille-quantise the accumulator. Quantising before
      // accumulating would stair-step every trail.
      ShaderEffect {
        anchors.fill: parent
        vertexShader: Qt.resolvedUrl("shaders/braille.vert.qsb")
        fragmentShader: Qt.resolvedUrl("shaders/display.frag.qsb")
        property variant fieldTex: accum
        property variant atlasTex: atlasTexture
        property vector4d grid: win.uGrid
        property vector4d audio: win.uAudio
        property vector4d anim: win.uAnim
        property vector4d view: win.uView
        property vector4d tint: win.uTint
        property vector4d fx: win.uFx
        property vector4d scene: win.uScene
        property vector4d sp0: win.uSp0
        property vector4d sp1: win.uSp1
        property vector4d sp2: win.uSp2
        property vector4d sp3: win.uSp3
        property vector4d sp4: win.uSp4
        property vector4d sp5: win.uSp5
        property vector4d sp6: win.uSp6
        property vector4d sp7: win.uSp7
        property vector4d music: win.uMusic
        property vector4d music2: win.uMusic2
        property vector4d rel: win.uRel
        property vector4d att: win.uAtt
        property vector4d pal0: win.uPal0
        property vector4d pal1: win.uPal1
        property vector4d pal2: win.uPal2
        property vector4d pal3: win.uPal3
      }

      Component.onCompleted: Qt.callLater(function() { atlasTexture.scheduleUpdate() })

      Item {
        anchors.fill: parent
        focus: true
        Keys.onPressed: function(event) {
          if (event.key === Qt.Key_Escape || event.text === "q") Qt.quit()
          else if (event.text === " ") root.phaseT = 0
          else if (event.text === "[") root.rowsTarget = Math.max(48, root.rowsTarget - 8)
          else if (event.text === "]") root.rowsTarget = Math.min(200, root.rowsTarget + 8)
          else if (event.text === "-") root.zoom = Math.max(0.40, root.zoom - 0.04)
          else if (event.text === "=") root.zoom = Math.min(1.30, root.zoom + 0.04)
          else if (event.text === ";") root.gain = Math.max(0.5, root.gain - 0.05)
          else if (event.text === "'") root.gain = Math.min(2.0, root.gain + 0.05)
          else if (event.text === "n") root.nextScene()
          else if (event.text === ",") {
            root.maxScene = Math.max(6, root.maxScene - 3)
            root.minScene = Math.min(root.minScene, root.maxScene - 2)
            console.log("scene pace: min=" + root.minScene + " max=" + root.maxScene)
          }
          else if (event.text === ".") {
            root.maxScene = Math.min(60, root.maxScene + 3)
            console.log("scene pace: min=" + root.minScene + " max=" + root.maxScene)
          }
          else if (event.text === "1") { root.sceneA = 0; root.sceneB = 0; root.sceneBlend = 0 }
          else if (event.text === "2") { root.sceneA = 1; root.sceneB = 1; root.sceneBlend = 0 }
          else if (event.text === "3") { root.sceneA = 2; root.sceneB = 2; root.sceneBlend = 0 }
          else if (event.text === "4") { root.sceneA = 3; root.sceneB = 3; root.sceneBlend = 0 }
          else if (event.text === "5") { root.sceneA = 4; root.sceneB = 4; root.sceneBlend = 0 }
          else if (event.text === "6") { root.sceneA = 5; root.sceneB = 5; root.sceneBlend = 0 }
          else if (event.text === "7") { root.sceneA = 6; root.sceneB = 6; root.sceneBlend = 0 }
          else if (event.text === "8") { root.sceneA = 7; root.sceneB = 7; root.sceneBlend = 0 }
          else if (event.text === "v") root.vignette = root.vignette > 0.7 ? 0.0 : root.vignette + 0.2
          else if (event.text === "e") root.edge = root.edge > 0.6 ? 0.0 : root.edge + 0.2
          else if (event.text === "p") console.log("look: rows=" + root.rowsTarget
                     + " zoom=" + root.zoom.toFixed(2) + " gain=" + root.gain.toFixed(2)
                     + " edge=" + root.edge.toFixed(2))
          else if (event.text === "d") {
            root.sequenceRunning = !root.sequenceRunning
            root.manualDissolve = root.sequenceRunning ? 0 : 1
          }
          else if (event.text === "t") {
            root.tintStep = (root.tintStep + 1) % root.tintSteps.length
            console.log("tint =", root.tintAmount)
          }
          event.accepted = true
        }
      }
    }
  }

  // ---------------------------------------------------------- sequencer
  // Each cover gets the same arc: assemble, hold, then a slow devolve that
  // carries it into the abstract field. phaseT restarts on every track change.
  readonly property real revealDuration: 2.0
  readonly property real holdDuration: 8.0
  readonly property real devolveDuration: 9.0
  property real phaseT: 0
  property bool sequenceRunning: true

  readonly property real dissolve: {
    if (!sequenceRunning) return manualDissolve
    var start = revealDuration + holdDuration
    if (phaseT <= start) return 0
    var k = Math.min(1, (phaseT - start) / devolveDuration)
    // Ease in so it creeps at first rather than lurching.
    return k * k * (3 - 2 * k)
  }
  property real manualDissolve: 0

  // ------------------------------------------------- live-tunable look
  // zoom: 1/aspect fits the cover to the height (whole cover, margins at the
  // sides); 1.0 bleeds it to full width (heavy crop). Anything between trades
  // composition against coverage.
  property real zoom: 0.54
  property real gain: 1.05
  property real edge: 0.32
  property int rowsTarget: 112
  property real vignette: 0.55

  // Dominant colours of the current cover, written alongside it by art-fetch.
  property var palette: ["#c8a06a", "#8a6f52", "#6a7a8a", "#c8c0b4"]
  FileView {
    path: root.revealedPath.length ? root.revealedPath + ".pal" : ""
    watchChanges: false
    printErrors: false
    onLoaded: {
      var lines = String(text()).split("\n").filter(function (l) { return l.trim().length > 0 })
      if (lines.length >= 4) root.palette = lines.slice(0, 4).map(function (l) { return l.trim() })
    }
  }
  function palColor(i) {
    var c = Qt.color(root.palette[i] || "#888888")
    var l = 0.299 * c.r + 0.587 * c.g + 0.114 * c.b
    // Lift dark entries toward a floor, keeping their hue. Without this a
    // near-black swatch paints a dead wedge across the frame.
    if (l < 0.42) {
      var k = 0.42 / Math.max(l, 0.04)
      return Qt.rgba(Math.min(1, c.r * k), Math.min(1, c.g * k), Math.min(1, c.b * k), 1)
    }
    return c
  }
  property real musicalTime: 0

  // ------------------------------------------------ preset director
  // A preset is an authored visual world, not a randomly selected effect.
  // The compatibility graph keeps successive worlds related by geometry and
  // motion, so the show develops instead of behaving like a shuffle reel.
  readonly property int sceneCount: 8
  // A scene runs at least minScene and at most maxScene. Inside that window,
  // structural novelty decides when -- and the change is held until the next
  // onset so it lands on a beat rather than mid-bar.
  property real minScene: 10.0
  property real maxScene: 16.0
  readonly property real sceneCrossfade: 2.8
  property int sceneA: 1
  property int sceneB: 4
  property real sceneBlend: 0
  property real sceneT: 0
  property bool changeArmed: false
  property real prevBarPhase: 0


  readonly property var presetNames: [
    "genesis", "tunnel", "interference", "rain",
    "spiral", "lattice", "contour", "streams"
  ]
  readonly property var presetSuccessors: [
    [1],          // genesis is debug-only; leave it immediately
    [4, 2],       // tunnel -> spiral or waves
    [6, 1],       // waves -> contour or tunnel
    [7, 6],       // rain -> streams or contour
    [7, 1],       // spiral -> streams or tunnel
    [6, 3],       // lattice -> contour or rain
    [2, 5],       // contour -> waves or lattice
    [4, 3]        // streams -> spiral or rain
  ]

  function drawScene(current) {
    var choices = root.presetSuccessors[current] || [0]
    if (choices.length === 1) return choices[0]

    // Percussive, energetic passages prefer the first successor. Harmonic or
    // quieter passages take the second. This is deliberately a small musical
    // decision, not randomness disguised as variety.
    var driving = root.perc > root.harm * 1.08 || root.slowEnergy > 0.62
    return choices[driving ? 0 : 1]
  }

  function nextScene() {
    if (sceneBlendAnim.running) return
    sceneBlendAnim.start()
  }

  NumberAnimation {
    id: sceneBlendAnim
    target: root
    property: "sceneBlend"
    from: 0
    to: 1
    duration: root.sceneCrossfade * 1000
    easing.type: Easing.InOutSine
    onFinished: {
      // Land on B, then arm a new B without disturbing what is on screen.
      root.sceneA = root.sceneB
      root.sceneB = root.drawScene(root.sceneA)
      root.sceneBlend = 0
      root.sceneT = 0
      console.log("preset:", root.presetNames[root.sceneA], "->", root.presetNames[root.sceneB])
    }
  }
  // Tint for monochrome covers. 0 = leave the art's own greys alone, which
  // reads better on most covers; raise toward 0.5 for a warmer look.
  property color tintColor: "#faa968"
  readonly property var tintSteps: [0.0, 0.25, 0.5, 0.85]
  property int tintStep: 0
  readonly property real tintAmount: tintSteps[tintStep]

  FrameAnimation {
    id: clock
    running: true
    property real t: 0
    onTriggered: {
      if (root.freeze) {
        // Fully deterministic A/B: freeze the clocks AND the audio, so the
        // only variable left is the beat.
        t = 12.0; root.musicalTime = root.freezeTime
        root.bass = 0.55; root.mid = 0.5; root.treble = 0.45; root.energy = 0.5
        root.slowEnergy = 0.5
        root.onset = 0
        if (!root.spec) {
          var sp = new Array(32)
          for (var i = 0; i < 32; i++) sp[i] = 0.35 + 0.45 * Math.abs(Math.sin(i * 0.7))
          root.spec = sp
        }
        return
      }
      t += frameTime
      root.phaseT += frameTime
      root.slowEnergy += (root.energy - root.slowEnergy) * frameTime * 0.28   // ~4s
      root.musicalTime += frameTime * (0.62 + root.slowEnergy * 1.05)
      if (root.dissolve >= 0.999) {
        root.sceneT += frameTime
        if (root.sceneT >= root.minScene) {
          if (!root.changeArmed
              && (root.novelty > root.novBase * 2.0 || root.sceneT >= root.maxScene))
            root.changeArmed = true
          // Cut on a bar line when the tracker is locked -- a change that
          // lands on the "1" reads as composed, where one landing on an
          // arbitrary onset reads as a glitch. Falls back to any strong onset
          // when there is no reliable tempo.
          if (root.changeArmed) {
            var onBar = root.beatConf > 0.45 && root.barPhase < root.prevBarPhase
            var onHit = root.beatConf <= 0.45 && root.onset > 0.6
            if (onBar || onHit) {
              root.changeArmed = false
              root.nextScene()
            }
          }
          root.prevBarPhase = root.barPhase
        }
      }
      root.onset = Math.max(0, root.onset - frameTime * 3.2)
      root.pulse = Math.max(0, root.pulse - frameTime * 1.3)
    }
  }
}
