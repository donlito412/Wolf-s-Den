#include "WDSamplePlayer.h"

namespace wolfsden
{

namespace
{
    inline float lerp (float a, float b, float t) noexcept { return a + (b - a) * t; }
}

//==============================================================================
WDSamplePlayer::WDSamplePlayer()
{
    formatManager_.registerBasicFormats();
}

WDSamplePlayer::~WDSamplePlayer() = default;

void WDSamplePlayer::prepare (double sampleRate, int maxBlockSize)
{
    sr       = sampleRate;
    maxBlock = maxBlockSize;
    readAheadBuf.setSize (2, kReadAheadFrames, false, true, false);
}

void WDSamplePlayer::reset()
{
    active   = false;
    noteHeld = false;
    readPos  = 0.0;
}

//==============================================================================
// Background loading

void WDSamplePlayer::requestLoad (int sampleId, const juce::String& filePath,
                                   int rootNoteMidi, bool loopEnabled, bool oneShot,
                                   float startFrac, float endFrac)
{
    juce::Thread::launch ([this, sampleId, filePath, rootNoteMidi,
                           loopEnabled, oneShot, startFrac, endFrac]
    {
        loadNow (sampleId, filePath, rootNoteMidi, loopEnabled, oneShot, startFrac, endFrac);
    });
}

void WDSamplePlayer::loadNow (int sampleId, const juce::String& filePath,
                               int rootNoteMidi, bool loopEnabled, bool oneShot,
                               float startFrac, float endFrac)
{
    // --- called on background thread ---
    const juce::File f (filePath);
    if (! f.existsAsFile()) return;

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager_.createReaderFor (f));
    if (! reader) return;

    const int64_t totalFrames = (int64_t) reader->lengthInSamples;
    const int     numChannels = (int) juce::jmin ((juce::uint32) reader->numChannels, (juce::uint32) 2u);
    const float   fileSizeMB  = (float) (totalFrames * numChannels * 4) / (1024.f * 1024.f);

    const int startFr = (int) (startFrac * (float) (totalFrames - 1));
    const int endFr   = (int) (endFrac   * (float) (totalFrames - 1));
    const int frames  = juce::jmax (1, endFr - startFr);

    if (fileSizeMB <= kMaxPreloadMB)
    {
        // Fully pre-load
        auto buf = std::make_unique<juce::AudioBuffer<float>> (numChannels, frames);
        reader->read (buf.get(), 0, frames, startFr, true, numChannels > 1);

        // Resample if reader SR differs from plugin SR
        if (reader->sampleRate > 0.0 && std::fabs (reader->sampleRate - sr) > 0.5)
        {
            const double ratio = sr / reader->sampleRate;
            const int newFrames = juce::roundToInt ((double) frames * ratio);
            juce::AudioBuffer<float> resampled (numChannels, newFrames);

            for (int ch = 0; ch < numChannels; ++ch)
            {
                juce::LagrangeInterpolator interp;
                interp.reset();
                const float* src = buf->getReadPointer (ch);
                float*       dst = resampled.getWritePointer (ch);
                interp.process (1.0 / ratio, src, dst, newFrames, frames, 0);
            }
            buf = std::make_unique<juce::AudioBuffer<float>> (std::move (resampled));
        }

        const std::lock_guard<std::recursive_mutex> lock (loadSwapMutex);
        rootNote_  = rootNoteMidi;
        looping    = loopEnabled;
        oneShot_   = oneShot;
        startFrame = 0;
        endFrame   = (int) (buf->getNumSamples() - 1);

        pendingBuffer = std::move (buf);
        bufferSwapReady.store (true, std::memory_order_release);
        loadedSampleId.store (sampleId, std::memory_order_release);
    }
    else
    {
        // Large file — keep reader for streaming
        const std::lock_guard<std::recursive_mutex> lock (loadSwapMutex);
        streamReader  = std::move (reader);
        streamReadPos = startFr;

        rootNote_  = rootNoteMidi;
        looping    = loopEnabled;
        oneShot_   = oneShot;
        startFrame = startFr;
        endFrame   = endFr;

        // Do not fill readAheadBuf here — loadNow runs on a background thread; audio thread reads
        // readAhead without lock in nextSampleMono/processBlock. Priming happens in startNote().
        loadedSampleId.store (sampleId, std::memory_order_release);
        // Audio thread clears cachedBuffer — cannot reset from here (render reads cache without lock).
        streamLoadNeedsCacheClear.store (true, std::memory_order_release);
    }
}

//==============================================================================
// Audio thread

bool WDSamplePlayer::hasReadableSource() const noexcept
{
    return cachedBuffer != nullptr || streamReader != nullptr;
}

void WDSamplePlayer::syncPendingToCache() noexcept
{
    swapBufferIfReady();
}

void WDSamplePlayer::swapBufferIfReady() noexcept
{
    if (streamLoadNeedsCacheClear.exchange (false, std::memory_order_acq_rel))
    {
        const std::lock_guard<std::recursive_mutex> lock (loadSwapMutex);
        cachedBuffer.reset();
        pendingBuffer.reset();
        bufferSwapReady.store (false, std::memory_order_release);
    }

    if (! bufferSwapReady.load (std::memory_order_acquire))
        return;

    const std::lock_guard<std::recursive_mutex> lock (loadSwapMutex);
    if (! bufferSwapReady.load (std::memory_order_relaxed))
        return;

    cachedBuffer = std::move (pendingBuffer);
    streamReader.reset();
    bufferSwapReady.store (false, std::memory_order_release);
    if (active) readPos = (double) startFrame;
}

void WDSamplePlayer::fillReadAhead() noexcept
{
    const std::lock_guard<std::recursive_mutex> lock (loadSwapMutex);
    if (! streamReader) return;
    const int numCh = (int) juce::jmin ((juce::uint32) streamReader->numChannels, (juce::uint32) 2u);
    // readAheadBuf sized in prepare(2, kReadAheadFrames) — avoid setSize here (RT heap alloc).
    streamReader->read (&readAheadBuf, 0, kReadAheadFrames, streamReadPos, true, numCh > 1);
    streamReadPos += kReadAheadFrames;
    if (looping && streamReadPos > endFrame)
        streamReadPos = startFrame;
}

void WDSamplePlayer::startNote (float pitchRatioFromRoot, float velocity) noexcept
{
    swapBufferIfReady();
    if (! cachedBuffer && ! streamReader) return;

    pitchRatio = (double) pitchRatioFromRoot;
    velocity_  = velocity;
    readPos    = (double) startFrame;
    if (streamReader)
    {
        streamReadPos = startFrame;
        fillReadAhead(); // audio thread only — avoids racing bg loader writes to readAheadBuf
    }
    active   = true;
    noteHeld = true;
}

void WDSamplePlayer::stopNote() noexcept
{
    noteHeld = false;
    if (! oneShot_) active = false;
}

bool WDSamplePlayer::processBlock (juce::AudioBuffer<float>& buffer,
                                    int chL, int chR,
                                    int startSample, int numSamples) noexcept
{
    swapBufferIfReady();
    if (! active) return false;

    const bool useStream = (streamReader != nullptr && ! cachedBuffer);
    const juce::AudioBuffer<float>* src = cachedBuffer ? cachedBuffer.get() : &readAheadBuf;
    if (! src || src->getNumSamples() == 0) return false;

    const int srcChannels    = src->getNumChannels();
    const int totalSrcFrames = src->getNumSamples();

    float* outL = buffer.getWritePointer (chL) + startSample;
    float* outR = (chR >= 0 && chR < buffer.getNumChannels())
                      ? buffer.getWritePointer (chR) + startSample
                      : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        const int   posI = (int) readPos;
        const float posF = (float) (readPos - (double) posI);

        if (posI >= totalSrcFrames - 1)
        {
            if (looping && noteHeld)
            {
                readPos = (double) startFrame;
                if (useStream) { streamReadPos = startFrame; fillReadAhead(); }
                continue;
            }
            else
            {
                active = false;
                break;
            }
        }

        const float s0L = src->getSample (0, posI);
        const float s1L = src->getSample (0, juce::jmin (posI + 1, totalSrcFrames - 1));
        const float sL  = lerp (s0L, s1L, posF) * velocity_;

        float sR = sL;
        if (srcChannels >= 2)
        {
            const float s0R = src->getSample (1, posI);
            const float s1R = src->getSample (1, juce::jmin (posI + 1, totalSrcFrames - 1));
            sR = lerp (s0R, s1R, posF) * velocity_;
        }

        outL[i] += sL;
        if (outR) outR[i] += sR;

        readPos += pitchRatio;

        if (useStream && (int) readPos >= kReadAheadFrames - (int) (pitchRatio + 1.0))
        {
            readPos -= (double) kReadAheadFrames;
            fillReadAhead();
        }
    }

    return active;
}

int WDSamplePlayer::getLoopFrameCount() const noexcept
{
    return juce::jmax (1, endFrame - startFrame + 1);
}

bool WDSamplePlayer::readMonoAt (double absoluteFrame, float& out) const noexcept
{
    out = 0.f;
    if (cachedBuffer == nullptr || cachedBuffer->getNumSamples() < 2)
        return false;

    const auto* src          = cachedBuffer.get();
    const int   totalSrcFrames = src->getNumSamples();
    const int   srcChannels    = src->getNumChannels();
    const int   span           = juce::jmax (1, endFrame - startFrame + 1);

    double f = absoluteFrame;
    if (looping)
    {
        while (f < (double) startFrame) f += (double) span;
        while (f > (double) endFrame) f -= (double) span;
    }
    else
        f = juce::jlimit ((double) startFrame, (double) endFrame, f);

    const int posI = juce::jlimit (0, totalSrcFrames - 2, (int) std::floor (f));
    const float frac = (float) (f - (double) posI);
    const int i1 = juce::jmin (posI + 1, totalSrcFrames - 1);

    float sL = lerp (src->getSample (0, posI), src->getSample (0, i1), frac);
    if (srcChannels >= 2)
    {
        const float sR = lerp (src->getSample (1, posI), src->getSample (1, i1), frac);
        sL = (sL + sR) * 0.5f;
    }
    out = sL;
    return true;
}

float WDSamplePlayer::nextSampleMono() noexcept
{
    swapBufferIfReady();
    if (! active) return 0.f;

    const bool useStream = (streamReader != nullptr && ! cachedBuffer);
    const juce::AudioBuffer<float>* src = cachedBuffer ? cachedBuffer.get() : &readAheadBuf;
    if (! src || src->getNumSamples() == 0) return 0.f;

    const int srcChannels    = src->getNumChannels();
    const int totalSrcFrames = src->getNumSamples();

    const int   posI = (int) readPos;
    const float posF = (float) (readPos - (double) posI);

    if (posI >= totalSrcFrames - 1)
    {
        if (looping && noteHeld)
        {
            readPos = (double) startFrame;
            if (useStream) { streamReadPos = startFrame; fillReadAhead(); }
            return 0.f;
        }
        else
        {
            active = false;
            return 0.f;
        }
    }

    const float s0L = src->getSample (0, posI);
    const float s1L = src->getSample (0, juce::jmin (posI + 1, totalSrcFrames - 1));
    float sL = (s0L + (s1L - s0L) * posF) * velocity_;

    float sR = sL;
    if (srcChannels >= 2)
    {
        const float s0R = src->getSample (1, posI);
        const float s1R = src->getSample (1, juce::jmin (posI + 1, totalSrcFrames - 1));
        sR = (s0R + (s1R - s0R) * posF) * velocity_;
    }

    readPos += pitchRatio;

    if (useStream && (int) readPos >= kReadAheadFrames - (int) (pitchRatio + 1.0))
    {
        readPos -= (double) kReadAheadFrames;
        fillReadAhead();
    }

    return (sL + sR) * 0.5f; // mono mix
}

} // namespace wolfsden
