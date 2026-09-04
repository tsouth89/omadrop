#include "native_scene_state.h"

#include <cassert>
#include <iostream>

namespace {
void finishTransition(NativeSceneDirector& director, const MusicFrame& music) {
    for (int frame = 0; frame < 360; ++frame) {
        if (!director.update(music, 1.0f / 60.0f).transitioning) return;
    }
    assert(false && "native scene transition did not finish");
}

NativeSceneKind automaticChoice(MusicFrame music) {
    NativeSceneDirector director;
    music.bpm = 120.0f;
    for (int frame = 0; frame < 520; ++frame) {
        director.update(music, 1.0f / 60.0f);
    }
    music.section = 1.0f;
    const NativeSceneState chosen = director.update(music, 1.0f / 60.0f);
    assert(chosen.transitioning);
    return chosen.incomingScene;
}
}

int main() {
    NativeSceneKind parsedScene = NativeSceneKind::DepthTunnel;
    assert(nativeSceneFromName("wire", parsedScene));
    assert(parsedScene == NativeSceneKind::WireOrganism);
    assert(nativeSceneFromName("prism-garden", parsedScene));
    assert(parsedScene == NativeSceneKind::PrismGarden);
    assert(nativeSceneFromName("constellation", parsedScene));
    assert(parsedScene == NativeSceneKind::ConstellationField);
    assert(nativeSceneFromName("bloom", parsedScene));
    assert(parsedScene == NativeSceneKind::BloomEngine);
    assert(!nativeSceneFromName("unknown", parsedScene));
    assert(nativeSceneMaterial(NativeSceneKind::DepthTunnel).fieldExposure
           != nativeSceneMaterial(NativeSceneKind::DepthTunnel).asciiExposure);
    assert(nativeSceneMaterial(NativeSceneKind::Centrifuge).fieldExposure
           != nativeSceneMaterial(NativeSceneKind::WireOrganism).fieldExposure);

    NativeSceneDirector director;
    MusicFrame music;
    music.bpm = 120.0f;
    music.energyFast = 0.18f;
    music.energySlow = 0.20f;
    for (int frame = 0; frame < 300; ++frame) {
        director.update(music, 1.0f / 60.0f);
    }
    const NativeSceneState developed = director.update(music, 1.0f / 60.0f);
    assert(developed.sceneBeats > 9.0f);
    assert(developed.development > 0.35f);
    assert(developed.drive < 0.1f);

    director.requestNext();
    const NativeSceneState startingTransition = director.update(music, 1.0f / 60.0f);
    assert(startingTransition.transitioning);
    assert(startingTransition.transition > 0.0f);
    assert(startingTransition.transition < 0.01f);
    assert(startingTransition.currentScene == NativeSceneKind::DepthTunnel);
    assert(startingTransition.incomingScene == NativeSceneKind::Centrifuge);
    float previousMix = startingTransition.transition;
    for (int frame = 0; frame < 30; ++frame) {
        const float mix = director.update(music, 1.0f / 60.0f).transition;
        assert(mix >= previousMix);
        previousMix = mix;
    }
    finishTransition(director, music);
    const NativeSceneState changed = director.update(music, 1.0f / 60.0f);
    assert(!changed.transitioning);
    assert(changed.currentScene == NativeSceneKind::Centrifuge);

    NativeSceneDirector timedDirector;
    timedDirector.setTransitionDuration(0.75f);
    timedDirector.requestNext();
    for (int frame = 0; frame < 30; ++frame) {
        assert(timedDirector.update(music, 1.0f / 60.0f).transitioning);
    }
    for (int frame = 0; frame < 30
         && timedDirector.state().transitioning; ++frame) {
        timedDirector.update(music, 1.0f / 60.0f);
    }
    assert(!timedDirector.state().transitioning);
    assert(timedDirector.state().currentScene == NativeSceneKind::Centrifuge);

    // A manual next request is one shot. It grants the requested scene a
    // minimum settled run, then the automatic director resumes on a musical
    // boundary without any hidden manual mode.
    NativeSceneDirector oneShotDirector;
    MusicFrame manualThenAutomatic = music;
    manualThenAutomatic.energyFast = 0.55f;
    manualThenAutomatic.energySlow = 0.50f;
    manualThenAutomatic.percussive = 0.45f;
    oneShotDirector.requestNext();
    oneShotDirector.update(manualThenAutomatic, 1.0f / 60.0f);
    finishTransition(oneShotDirector, manualThenAutomatic);
    for (int frame = 0; frame < 450; ++frame) {
        manualThenAutomatic.section = 0.0f;
        oneShotDirector.update(manualThenAutomatic, 1.0f / 60.0f);
    }
    manualThenAutomatic.section = 1.0f;
    assert(!oneShotDirector.update(
        manualThenAutomatic, 1.0f / 60.0f).transitioning);
    manualThenAutomatic.section = 0.0f;
    for (int frame = 0; frame < 40; ++frame) {
        oneShotDirector.update(manualThenAutomatic, 1.0f / 60.0f);
    }
    manualThenAutomatic.section = 1.0f;
    assert(oneShotDirector.update(
        manualThenAutomatic, 1.0f / 60.0f).transitioning);
    finishTransition(oneShotDirector, manualThenAutomatic);

    director.requestNext();
    director.update(music, 1.0f / 60.0f);
    finishTransition(director, music);
    assert(director.update(music, 1.0f / 60.0f).currentScene
           == NativeSceneKind::WireOrganism);
    director.requestNext();
    director.update(music, 1.0f / 60.0f);
    finishTransition(director, music);
    assert(director.update(music, 1.0f / 60.0f).currentScene
           == NativeSceneKind::PrismGarden);
    director.requestPrevious();
    director.update(music, 1.0f / 60.0f);
    finishTransition(director, music);
    assert(director.update(music, 1.0f / 60.0f).currentScene
           == NativeSceneKind::WireOrganism);

    music.energyFast = 0.86f;
    music.energySlow = 0.72f;
    music.percussive = 0.70f;
    for (int frame = 0; frame < 180; ++frame) {
        director.update(music, 1.0f / 60.0f);
    }
    const NativeSceneState driving = director.update(music, 1.0f / 60.0f);
    assert(driving.drive > 0.8f);
    assert(driving.peak > 0.8f);

    music.energySlope = -0.5f;
    for (int frame = 0; frame < 90; ++frame) {
        director.update(music, 1.0f / 60.0f);
    }
    const NativeSceneState releasing = director.update(music, 1.0f / 60.0f);
    assert(releasing.release > 0.9f);

    music.section = 1.0f;
    const NativeSceneState reset = director.update(music, 1.0f / 60.0f);
    assert(reset.sceneBeats > 8.0f);
    assert(!reset.transitioning);
    director.reset();
    director.selectScene(NativeSceneKind::Centrifuge);
    assert(director.update(MusicFrame{}, 1.0f / 60.0f).currentScene
           == NativeSceneKind::Centrifuge);
    director.reset();
    const NativeSceneState fresh = director.update(MusicFrame{}, 1.0f / 60.0f);
    assert(fresh.sceneBeats < 0.1f);

    NativeSceneDirector recurrenceDirector;
    MusicFrame recurrenceMusic;
    recurrenceMusic.bpm = 120.0f;
    recurrenceMusic.motifIdentity = 7;
    recurrenceMusic.section = 1.0f;
    const NativeSceneState firstMotif = recurrenceDirector.update(
        recurrenceMusic, 1.0f / 60.0f);
    assert(!firstMotif.motifRecalled);
    recurrenceMusic.section = 0.0f;
    recurrenceDirector.update(recurrenceMusic, 1.0f / 60.0f);
    recurrenceDirector.requestNext();
    recurrenceDirector.update(recurrenceMusic, 1.0f / 60.0f);
    finishTransition(recurrenceDirector, recurrenceMusic);
    assert(recurrenceDirector.update(recurrenceMusic, 1.0f / 60.0f).currentScene
           == NativeSceneKind::Centrifuge);
    recurrenceMusic.section = 1.0f;
    const NativeSceneState recalled = recurrenceDirector.update(
        recurrenceMusic, 1.0f / 60.0f);
    assert(recalled.motifRecalled);
    assert(recalled.transitioning);
    assert(recalled.incomingScene == NativeSceneKind::DepthTunnel);

    NativeSceneDirector trackResetDirector;
    trackResetDirector.selectScene(NativeSceneKind::WireOrganism);
    trackResetDirector.resetForTrack();
    assert(trackResetDirector.state().currentScene == NativeSceneKind::WireOrganism);
    assert(!trackResetDirector.state().transitioning);
    trackResetDirector.requestNext();
    trackResetDirector.update(recurrenceMusic, 1.0f / 60.0f);
    assert(trackResetDirector.state().transitioning);
    trackResetDirector.resetForTrack();
    assert(trackResetDirector.state().currentScene == NativeSceneKind::PrismGarden);
    assert(!trackResetDirector.state().transitioning);

    NativeSceneDirector textureDirector;
    MusicFrame drivingTexture;
    drivingTexture.bpm = 120.0f;
    drivingTexture.energyFast = 0.92f;
    drivingTexture.energySlow = 0.82f;
    drivingTexture.percussive = 0.92f;
    drivingTexture.harmonic = 0.24f;
    drivingTexture.spectralCentroid = 0.68f;
    drivingTexture.stereoWidth = 0.42f;
    for (int frame = 0; frame < 520; ++frame) {
        textureDirector.update(drivingTexture, 1.0f / 60.0f);
    }
    drivingTexture.motifIdentity = 31;
    drivingTexture.section = 1.0f;
    const NativeSceneState textureChoice = textureDirector.update(
        drivingTexture, 1.0f / 60.0f);
    assert(textureChoice.transitioning);
    assert(textureChoice.incomingScene == NativeSceneKind::Centrifuge);

    MusicFrame calmHarmonic;
    calmHarmonic.energyFast = 0.25f;
    calmHarmonic.energySlow = 0.25f;
    calmHarmonic.percussive = 0.15f;
    calmHarmonic.harmonic = 0.90f;
    calmHarmonic.spectralCentroid = 0.15f;
    calmHarmonic.stereoWidth = 0.65f;
    assert(automaticChoice(calmHarmonic) == NativeSceneKind::TidalGrid);

    MusicFrame wideHarmonic;
    wideHarmonic.energyFast = 0.60f;
    wideHarmonic.energySlow = 0.60f;
    wideHarmonic.percussive = 0.42f;
    wideHarmonic.harmonic = 0.74f;
    wideHarmonic.spectralCentroid = 0.54f;
    wideHarmonic.stereoWidth = 0.95f;
    assert(automaticChoice(wideHarmonic) == NativeSceneKind::OrbitalLoom);

    MusicFrame sparseBright;
    sparseBright.energyFast = 0.22f;
    sparseBright.energySlow = 0.22f;
    sparseBright.percussive = 0.18f;
    sparseBright.harmonic = 0.72f;
    sparseBright.spectralCentroid = 0.74f;
    sparseBright.stereoWidth = 0.76f;
    assert(automaticChoice(sparseBright)
           == NativeSceneKind::ConstellationField);

    NativeSceneDirector fallbackDirector;
    MusicFrame fallbackMusic;
    fallbackMusic.bpm = 120.0f;
    fallbackMusic.energyFast = 0.46f;
    fallbackMusic.energySlow = 0.42f;
    fallbackMusic.harmonic = 0.62f;
    bool fallbackStarted = false;
    for (int frame = 0; frame < 1100 && !fallbackStarted; ++frame) {
        fallbackMusic.barPhase = std::fmod(frame / 120.0f, 1.0f);
        fallbackMusic.phrasePhase = std::fmod(frame / 480.0f, 1.0f);
        fallbackStarted = fallbackDirector.update(
            fallbackMusic, 1.0f / 60.0f).transitioning;
    }
    assert(fallbackStarted);

    NativeSceneDirector silentDirector;
    MusicFrame silentMusic;
    silentMusic.bpm = 120.0f;
    for (int frame = 0; frame < 2100; ++frame) {
        silentMusic.barPhase = std::fmod(frame / 120.0f, 1.0f);
        silentMusic.phrasePhase = std::fmod(frame / 480.0f, 1.0f);
        assert(!silentDirector.update(
            silentMusic, 1.0f / 60.0f).transitioning);
    }
    std::cout << "native scene state passed\n";
}
