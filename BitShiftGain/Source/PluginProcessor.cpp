#include "PluginProcessor.h"

AudioProcessor::AudioProcessor() :
    juce::AudioProcessor(BusesProperties().withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                                          .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

AudioProcessor::~AudioProcessor()
{
}

void AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    resetState();
}

void AudioProcessor::releaseResources()
{
}

void AudioProcessor::reset()
{
    resetState();
}

bool AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void AudioProcessor::resetState()
{
    gain = 1.0f;
}

template <typename NumericType, size_t MinMaxValue>
struct GainLookup {
  constexpr static auto generate_gains() {
    constexpr auto total_size = (2 * MinMaxValue) + 1;
    std::array<NumericType, total_size> g{};
    constexpr auto middle_index = MinMaxValue;
    g[middle_index] = 1.0f;
    for (size_t i = 1; i <= (total_size / 2); ++i) {
      auto y = size_t(1) << i;
      auto upper_index = i + middle_index;
      auto lower_index = middle_index - i;
      auto as_t = static_cast<NumericType>(y);
      g[upper_index] = as_t;
      g[lower_index] = 1.0f / as_t;
    }
    return g;
  }
  constexpr static auto gains = generate_gains();

  constexpr static auto from_bits(int bit) noexcept {
    auto index = static_cast<uint32_t>(bit + static_cast<int>(MinMaxValue));
    return gains[index];
  }
};

static_assert(GainLookup<float, 16>::from_bits(-16) == 0.0000152587890625f);
static_assert(GainLookup<float, 16>::from_bits(-15) == 0.000030517578125f);
// ...
static_assert(GainLookup<float, 16>::from_bits(0) == 1.0f);
static_assert(GainLookup<float, 16>::from_bits(1) == 2.0f);
static_assert(GainLookup<float, 16>::from_bits(2) == 4.0f);
static_assert(GainLookup<float, 16>::from_bits(3) == 8.0f);
static_assert(GainLookup<float, 16>::from_bits(4) == 16.0f);
static_assert(GainLookup<float, 16>::from_bits(5) == 32.0f);
static_assert(GainLookup<float, 16>::from_bits(6) == 64.0f);
static_assert(GainLookup<float, 16>::from_bits(7) == 128.0f);
static_assert(GainLookup<float, 16>::from_bits(8) == 256.0f);
// ...
static_assert(GainLookup<float, 16>::from_bits(16) == 65536.0f);

void AudioProcessor::update()
{
    int bits = apvts.getRawParameterValue("BitShift")->load();
    gain = GainLookup<float, 16>::from_bits(bits);
}

void AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any output channels that don't contain input data.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i) {
        buffer.clear(i, 0, buffer.getNumSamples());
    }

    update();

    const float* inL = buffer.getReadPointer(0);
    const float* inR = buffer.getReadPointer(1);
    float* outL = buffer.getWritePointer(0);
    float* outR = buffer.getWritePointer(1);

    for (int i = 0; i < buffer.getNumSamples(); ++i) {
        outL[i] = inL[i] * gain;
        outR[i] = inR[i] * gain;
    }
}

juce::AudioProcessorEditor* AudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor(*this);
}

void AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    copyXmlToBinary(*apvts.copyState().createXml(), destData);
}

void AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml.get() != nullptr && xml->hasTagName(apvts.state.getType())) {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout AudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID("BitShift", 1),
        "BitShift",
        -16, 16, 0,
        juce::AudioParameterIntAttributes().withLabel("bits")));

    return layout;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioProcessor();
}
