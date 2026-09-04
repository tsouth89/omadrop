#include "mpris_state.h"
#include "paired_display.h"
#include "paired_music_state.h"
#include "structure_timeline.h"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    assert(argc == 2);
    StructureTimeline timeline;
    std::string error;
    assert(timeline.load(argv[1], error));
    assert(timeline.sections().size() == 5);
    assert(timeline.appliesTo("fixture:any-song"));
    assert(timeline.sectionAt(0.0) == 0);
    assert(timeline.sectionAt(19.999) == 0);
    assert(timeline.sectionAt(20.0) == 1);
    assert(timeline.sectionAt(40.0) == 2);
    assert(!timeline.sectionAt(100.0));

    TimelineDirector director;
    std::size_t nextChoice = 1;
    auto familyForPreset = [](std::size_t preset) { return static_cast<int>(preset % 3); };
    auto choose = [&](int preferredFamily) {
        return preferredFamily >= 0 ? static_cast<std::size_t>(preferredFamily + 3)
                                    : nextChoice++;
    };
    const auto intro = director.sync(timeline, 2.0, 4, 6, false, choose, familyForPreset);
    assert(intro && intro->initial && intro->targetPreset == 4);
    const auto verse = director.sync(timeline, 21.0, 4, 6, false, choose, familyForPreset);
    assert(verse && verse->sectionChanged && verse->targetPreset == 1);
    const auto chorus = director.sync(timeline, 41.0, 1, 6, false, choose, familyForPreset);
    assert(chorus && chorus->targetPreset == 2);
    const auto repeatedVerse = director.sync(
        timeline, 61.0, 2, 6, false, choose, familyForPreset);
    assert(repeatedVerse && repeatedVerse->targetPreset == 4);
    assert(familyForPreset(repeatedVerse->targetPreset) == familyForPreset(verse->targetPreset));
    const auto repeatedChorus = director.sync(
        timeline, 81.0, 4, 6, true, choose, familyForPreset);
    assert(repeatedChorus && repeatedChorus->hardSync && repeatedChorus->targetPreset == 5);
    assert(familyForPreset(repeatedChorus->targetPreset) == familyForPreset(chorus->targetPreset));

    director.reset();
    const auto reset = director.sync(timeline, 41.0, 5, 6, false, choose, familyForPreset);
    assert(reset && reset->initial && reset->targetPreset == 5);

    director.reset();
    const auto firstMotif = director.sync(
        timeline, 2.0, 0, 6, false, [](int) { return 0; }, familyForPreset);
    const auto collision = director.sync(
        timeline, 21.0, 0, 6, false, [](int) { return 0; }, familyForPreset);
    assert(firstMotif && firstMotif->targetPreset == 0);
    assert(collision && collision->targetPreset == 0);

    const std::string stateJson = R"JSON({
        "identity":"spotify:track:test",
        "playback_status":"Playing",
        "art_path":"/tmp/cover.png",
        "position_seconds":12.5,
        "duration_seconds":100.0
    })JSON";
    const auto state = parseMprisState(stateJson, error);
    assert(state && state->identity == "spotify:track:test");
    PlaybackClock clock;
    const auto first = clock.observe(*state, 50.0);
    assert(first.first && !first.trackChanged && !first.seeked);
    assert(std::abs(clock.positionAt(50.5) - 13.0) < 0.001);

    MprisState normal = *state;
    normal.positionSeconds = 13.5;
    const auto continuous = clock.observe(normal, 51.0);
    assert(!continuous.seeked);
    MprisState seek = normal;
    seek.positionSeconds = 60.0;
    const auto jumped = clock.observe(seek, 51.5);
    assert(jumped.seeked && !jumped.trackChanged);
    MprisState nextTrack = seek;
    nextTrack.identity = "spotify:track:next";
    nextTrack.positionSeconds = 0.0;
    const auto changed = clock.observe(nextTrack, 52.0);
    assert(changed.trackChanged && !changed.seeked);

    StructureTimeline invalid;
    const auto fixtureDirectory = std::filesystem::path(argv[1]).parent_path();
    StructureTimeline bound;
    assert(bound.load(fixtureDirectory / "bound-song.json", error));
    assert(bound.appliesTo("fixture:bound-song"));
    assert(!bound.appliesTo(""));
    assert(!bound.appliesTo("fixture:another-song"));
    const auto invalidPath = fixtureDirectory / "invalid-overlap.json";
    assert(!invalid.load(invalidPath, error));
    assert(error.find("non-overlapping") != std::string::npos);

    PairedDisplayFollower pairedFollower;
    const auto pairedTransition = pairedFollower.consume("1 7 4200 2 0\n", 16);
    assert(pairedTransition && pairedTransition->presetIndex == 7);
    assert(pairedTransition->durationMs == 4200);
    assert(pairedTransition->transitionMode == 2);
    assert(!pairedTransition->hardSync);
    assert(!pairedFollower.consume("1 7 4200 2 0\n", 16));
    const auto pairedHardSync = pairedFollower.consume("2 4 0 0 1\n", 16);
    assert(pairedHardSync && pairedHardSync->presetIndex == 4);
    assert(pairedHardSync->hardSync);
    const auto nativeTransition = pairedFollower.consume(
        "3 4 0 6 0 2 1\n", 16, 3);
    assert(nativeTransition && nativeTransition->nativeScene == 2);
    assert(nativeTransition->nativeSourceScene == 1);
    assert(nativeTransition->transitionMode == 6);
    const auto pairedControls = pairedFollower.consume(
        "4 4 0 6 1 2 2 0 1 230 0\n", 16, 3);
    assert(pairedControls && pairedControls->asciiMode == 0);
    assert(pairedControls->fullscreenMode == 1);
    assert(pairedControls->syncDelayMs == 230);
    assert(pairedControls->closeMode == 0);
    assert(!decodePairedDisplayState(
        "5 4 0 6 1 2 2 2 1 230\n", 16, 3));
    assert(!decodePairedDisplayState("4 4 0 6 0 3\n", 16, 3));
    assert(!decodePairedDisplayState("3 16 0 0 1\n", 16));

    MusicFrame sharedMusic;
    sharedMusic.audioTimeSeconds = 42.25;
    sharedMusic.bpm = 127.0f;
    sharedMusic.kick = 0.82f;
    sharedMusic.spectrumLevel[12] = 1.4f;
    PairedMusicFollower musicFollower;
    const std::string encodedMusic = encodePairedMusicState({
        .serial = 9,
        .frame = sharedMusic,
    });
    const auto receivedMusic = musicFollower.consume(encodedMusic);
    assert(receivedMusic && receivedMusic->audioTimeSeconds == 42.25);
    assert(receivedMusic->bpm == 127.0f);
    assert(receivedMusic->kick == 0.82f);
    assert(receivedMusic->spectrumLevel[12] == 1.4f);
    assert(!musicFollower.consume(encodedMusic));
    assert(!decodePairedMusicState("invalid"));

    std::cout << "structure timeline passed\n";
}
