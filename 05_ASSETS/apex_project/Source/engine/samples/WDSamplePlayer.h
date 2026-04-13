#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include <atomic>
#include <memory>
#include <mutex>

namespace wolfsden
{

/** Per-VoiceLayer sample playback engine — replaces the wavetable stub
 *  for osc_type == 7 ("Sample").
 *
 *  Thread model:
 *    - requestLoad() is real-time safe — queues background loading via juce::Thread::launch.
 *    - prepare(), startNote(), stopNote(), processBlock() are called from the audio thread only.
 *    - loadNow() is called on a background thread (never from the audio thread).
 *
 *  Memory model:
 *    - Samples ≤ kMaxPreloadMB are fully pre-loaded (keeps disk off the audio thread).
 *    - Larger files stream via AudioFormatReader + read-ahead (may cost CPU / occasional glitches).
 *
 *  The caller (VoiceLayer) applies the amplitude ADSR on top of processBlock() output.
 */
class WDSamplePlayer
{
public:
    WDSamplePlayer();
    ~WDSamplePlayer();

    /** Called from audio thread at plugin prepare time. */
    void prepare (double sampleRate, int maxBlockSize);

    /** Reset all playback state (note off, clear position). */
    void reset();

    /** Queue a new sample to load by ID + file path.  Real-time safe. */
    void requestLoad (int sampleId, const juce::String& filePath,
                      int rootNoteMidi, bool loopEnabled, bool oneShot,
                      float startFrac, float endFrac);

    /** Trigger a new note.  pitchRatioFromRoot: 1.0 = root note pitch. */
    void startNote (float pitchRatioFromRoot, float velocity) noexcept;

    /** Begin release — one-shot samples ignore this until their end. */
    void stopNote() noexcept;

    /** Write sample audio into buffer[chL] and buffer[chR].
     *  Returns false when playback has finished. */
    bool processBlock (juce::AudioBuffer<float>& buffer,
                       int chL, int chR,
                       int startSample, int numSamples) noexcept;

    /** Per-sample mono output for use in the per-sample render loop.
     *  Returns 0 if not active. Advances the read position by pitchRatio.
     *  Stereo samples are averaged to mono; the stereo field is spread via pan in renderAdd. */
    float nextSampleMono() noexcept;

    bool isActive()  const noexcept { return active; }
    bool hasBuffer() const noexcept { return cachedBuffer != nullptr; }

    /** True if a preloaded cache or streaming reader is available (audio thread). */
    bool hasReadableSource() const noexcept;

    /** Promote a finished background load into the cache before noteOn / rendering. */
    void syncPendingToCache() noexcept;

    /** ID of the currently loaded sample (0 = none). */
    int loadedId() const noexcept { return loadedSampleId.load(); }

    /** MIDI root note of the currently loaded sample (C4 = 60). */
    int rootNote() const noexcept { return rootNote_; }

    /** Called on the background loading thread — not from audio thread. */
    void loadNow (int sampleId, const juce::String& filePath,
                  int rootNoteMidi, bool loopEnabled, bool oneShot,
                  float startFrac, float endFrac);

    static constexpr int   kReadAheadFrames = 1024;
    /** Preload typical instrument WAVs; streaming uses sync disk reads on the audio thread (can crackle). */
    static constexpr float kMaxPreloadMB    = 48.0f;

private:
    double sr       = 44100.0;
    int    maxBlock = 512;

    // Playback state (audio thread)
    bool   active    = false;
    bool   noteHeld  = false;
    bool   looping   = false;
    bool   oneShot_  = false;
    double readPos   = 0.0;
    double pitchRatio = 1.0;
    float  velocity_ = 1.0f;
    int    startFrame = 0;
    int    endFrame   = 0;
    int    rootNote_  = 60;

    // Loaded sample data (swap via pointer + atomic flag)
    std::unique_ptr<juce::AudioBuffer<float>> cachedBuffer;
    std::unique_ptr<juce::AudioBuffer<float>> pendingBuffer;
    std::atomic<bool> bufferSwapReady { false };
    /** Background streaming load finished — audio thread must drop preload cache before using streamReader. */
    std::atomic<bool> streamLoadNeedsCacheClear { false };
    std::atomic<int>  loadedSampleId  { 0 };

    // Streaming (large files)
    std::unique_ptr<juce::AudioFormatReader> streamReader;
    juce::AudioBuffer<float> readAheadBuf;
    int64_t streamReadPos = 0;

    juce::AudioFormatManager formatManager_;

    /** Protects pending/cached buffer, streamReader, and swap flag (bg load vs audio thread). */
    mutable std::recursive_mutex loadSwapMutex;

    void swapBufferIfReady() noexcept;
    void fillReadAhead() noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WDSamplePlayer)
};

} // namespace wolfsden
