#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <functional>

namespace sa
{
// M1 audible spine: load a sample, decode it fully into memory, draw it (thumbnail), and
// trigger it instantly through the audio device. Decoding to an in-RAM buffer (rather than
// disk streaming) gives zero-latency one-shot triggering and is the buffer that M2's
// trim/shape will operate on.
//
// Note: on the MacBook built-in speakers the amp sleeps during silence, so the very first
// hit after a quiet spell can lag ~100-300 ms (a hardware quirk, not engine latency — our
// measured output latency is ~28 ms). It does not occur on headphones / an audio interface.
// A "keep the output warm" mitigation can be added later if needed.
class AudioEngine : private juce::ChangeListener
{
public:
    AudioEngine();
    ~AudioEngine() override;

    bool loadFile (const juce::File& file);
    void play();
    void stop();
    void togglePlay();
    bool isPlaying() const;

    bool hasFile() const                      { return sampleBuffer.getNumSamples() > 0; }
    juce::AudioThumbnail& thumbnail()         { return thumb; }

    juce::String  fileName() const            { return loadedName; }
    double        sampleRate() const          { return fileSampleRate; }
    int           bitDepth() const            { return fileBits; }
    juce::int64   lengthFrames() const        { return fileFrames; }
    double        lengthSeconds() const       { return transport.getLengthInSeconds(); }
    double        positionSeconds() const     { return transport.getCurrentPosition(); }

    std::function<void()> onChange;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;

    juce::AudioDeviceManager deviceManager;
    juce::AudioFormatManager formatManager;
    juce::AudioSourcePlayer sourcePlayer;
    juce::AudioTransportSource transport;

    juce::AudioBuffer<float> sampleBuffer;                       // decoded sample, in RAM
    std::unique_ptr<juce::PositionableAudioSource> playSource;   // plays from sampleBuffer

    juce::AudioThumbnailCache thumbCache { 2 };
    juce::AudioThumbnail thumb { 512, formatManager, thumbCache };

    juce::String loadedName;
    double  fileSampleRate = 0.0;
    int     fileBits = 0;
    juce::int64 fileFrames = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};
}
