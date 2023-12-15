/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
CircularBuffer::CircularBuffer()
{
}

void CircularBuffer::Init(int size)
{
	m_head = 0;
	m_size = size;
	m_buffer.setSize(1, size);
}

void CircularBuffer::Clear()
{
	m_head = 0;
	m_buffer.clear();
}

float CircularBuffer::ReadDelay(float sample)
{
	const int bufferSize = m_size;
	const float readIdx = m_head + bufferSize - sample;

	const int flr = static_cast<int>(readIdx);
	const int iPrev = flr < bufferSize ? flr : flr - bufferSize;
	int iNext = flr + 1;
	iNext = iNext < bufferSize ? iNext : iNext - bufferSize;

	const float weight = readIdx - flr;
	return m_buffer.getSample(0, iPrev) * (1.f - weight) + m_buffer.getSample(0, iNext) * weight;
}

float CircularBuffer::ReadFactor(float factor)
{
	float sample = 2.0f + m_size * factor * 0.98f;
	return ReadDelay(sample);
}

//==============================================================================
DelayLineDifuser::DelayLineDifuser()
{
	for (int stage = 0; stage < N_STAGES; stage++)
	{
		for (int delayLine = 0; delayLine < N_DELAY_LINES; delayLine++)
		{
			m_buffer[stage][delayLine] = CircularBuffer();
		}
	}
}

void DelayLineDifuser::Init(float delayFactor, int sampleRate)
{
	const float sampleFactor = delayFactor * sampleRate * 0.001f;
	float default[N_DELAY_LINES] = { 0.49f, 1.41f, 6.85f, 11.23f };

	for (int stage = 0; stage < N_STAGES; stage++)
	{
		for (int delayLine = 0; delayLine < N_DELAY_LINES; delayLine++)
		{
			float factor = default[delayLine] * (0.87f + stage);
			m_buffer[stage][delayLine].Init(1 + (int)(sampleFactor * factor));
		}
	}
}

float DelayLineDifuser::ProcessSample(float inSample, float factor, int density)
{
	// Clamp density
	int densitySafe = density;
	if (densitySafe > N_STAGES)
		densitySafe = N_STAGES;
	if (densitySafe < 2)
		densitySafe = 2;

	float delayIn[N_DELAY_LINES];
	float delayOut[N_DELAY_LINES];

	float dryMix = 0.0f;

	delayIn[0] = 0.8f * inSample;
	delayIn[1] = 1.2f * inSample;
	delayIn[2] = -inSample - 0.1f;
	delayIn[3] = -inSample + 0.1f;

	for (int stage = 0; stage < densitySafe; stage++)
	{
		for (int delayLine = 0; delayLine < N_DELAY_LINES; delayLine++)
		{
			m_buffer[stage][delayLine].WriteSample(delayIn[delayLine]);
			delayOut[delayLine] = m_buffer[stage][delayLine].ReadFactor(factor);
		}

		dryMix = (1.0f - stage / densitySafe) * 0.5f;

		delayIn[0] = dryMix * inSample + delayOut[0] + delayOut[1] + delayOut[2] + delayOut[3];
		delayIn[1] = dryMix * inSample + delayOut[0] - delayOut[1] + delayOut[2] - delayOut[3];
		delayIn[2] = dryMix * inSample + delayOut[0] + delayOut[1] - delayOut[2] - delayOut[3];
		delayIn[3] = dryMix * inSample + delayOut[0] - delayOut[1] - delayOut[2] + delayOut[3];
	}
	// TO DO: Better volume conpensation
	return 0.015f * (delayIn[0] + delayIn[1] + delayIn[2] + delayIn[3]) * (1.0f - (densitySafe / N_STAGES) * 0.75f);
}

void DelayLineDifuser::Clear()
{
	for (int stage = 0; stage < N_STAGES; stage++)
	{
		for (int delayLine = 0; delayLine < N_DELAY_LINES; delayLine++)
		{
			m_buffer[stage][delayLine].Clear();
		}
	}
}

//==============================================================================
EnvelopeFollower::EnvelopeFollower()
{
}

void EnvelopeFollower::Init(int sampleRate)
{
	m_SampleRate = sampleRate;
}

void EnvelopeFollower::SetCoef(float attackTime, float releaseTime)
{
	m_AttackCoef = expf(-1000.0f / (attackTime * m_SampleRate));
	m_ReleaseCoef = expf(-1000.0f / (releaseTime * m_SampleRate));
}

float EnvelopeFollower::process(float in)
{
	const float tmp = fabs(in);
	if (tmp > m_Envelope)
	{
		return m_Envelope = tmp + m_AttackCoef * (m_Envelope - tmp);
	}
	else
	{
		return m_Envelope = tmp + m_ReleaseCoef * (m_Envelope - tmp);
	}
}

//==============================================================================

const std::string DifuserAudioProcessor::paramsNames[] = { "Lenght", "Density", "Threshold", "Mix", "Volume" };

//==============================================================================
DifuserAudioProcessor::DifuserAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
	difusionLenghtParameter = apvts.getRawParameterValue(paramsNames[0]);
	densityParameter		= apvts.getRawParameterValue(paramsNames[1]);
	thresholdParameter		= apvts.getRawParameterValue(paramsNames[2]);
	mixParameter			= apvts.getRawParameterValue(paramsNames[3]);
	volumeParameter			= apvts.getRawParameterValue(paramsNames[4]);
}

DifuserAudioProcessor::~DifuserAudioProcessor()
{
}

//==============================================================================
const juce::String DifuserAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool DifuserAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool DifuserAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool DifuserAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double DifuserAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int DifuserAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int DifuserAudioProcessor::getCurrentProgram()
{
    return 0;
}

void DifuserAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String DifuserAudioProcessor::getProgramName (int index)
{
    return {};
}

void DifuserAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void DifuserAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
	// Maximum diffusion lenght
	float difusionLenght = 5.0f;
	m_delayLineDifuser[0].Init(difusionLenght, (int)(sampleRate));
	m_delayLineDifuser[0].Clear();
	m_delayLineDifuser[1].Init(difusionLenght, (int)(sampleRate));
	m_delayLineDifuser[1].Clear();

	m_envelopeFollower[0].Init((int)(sampleRate));
	m_envelopeFollower[1].Init((int)(sampleRate));

	const float attack = 10;
	const float release = 200;

	m_envelopeFollower[0].SetCoef(attack, release);
	m_envelopeFollower[1].SetCoef(attack, release);
}

void DifuserAudioProcessor::releaseResources()
{
	m_delayLineDifuser[0].Clear();
	m_delayLineDifuser[1].Clear();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool DifuserAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void DifuserAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
	// Parameters
	const float factor = difusionLenghtParameter->load();
	const int density = (int)(densityParameter->load());
	const float mix = mixParameter->load();
	const float volume = juce::Decibels::decibelsToGain(volumeParameter->load());
	const float thresholddB = thresholdParameter->load();
	const float threshold = juce::Decibels::decibelsToGain(thresholddB);
	
	// Mics constants
	const float mixInverse = 1.0f - mix;
	const int channels = getTotalNumOutputChannels();
	const int samples = buffer.getNumSamples();
	
	for (int channel = 0; channel < channels; ++channel)
	{
		auto* channelBuffer = buffer.getWritePointer(channel);

		auto& delayLineDifuser = m_delayLineDifuser[channel];
		auto& envelopeFollower = m_envelopeFollower[channel];

		for (int sample = 0; sample < samples; ++sample)
		{
			const float in = channelBuffer[sample];

			float inDifuse = delayLineDifuser.ProcessSample(in, factor, density);

			float envelopedB  = juce::Decibels::gainToDecibels(envelopeFollower.process(inDifuse));

			// Calculate mix ratio
			float dynamicMix = 0.0f;
			if (envelopedB > thresholddB)
			{
				dynamicMix = fminf((envelopedB - thresholddB) / 12.0f, 1.0f);
			}

			// Apply dynamic mix ratio
			const float inDifuseDynamic  = dynamicMix * inDifuse + (1.0f - dynamicMix) * in;

			// Static mix
			channelBuffer[sample]  = volume * (mix * inDifuseDynamic + mixInverse * in);
		}
	}
}

//==============================================================================
bool DifuserAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* DifuserAudioProcessor::createEditor()
{
    return new DifuserAudioProcessorEditor (*this, apvts);
}

//==============================================================================
void DifuserAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
	auto state = apvts.copyState();
	std::unique_ptr<juce::XmlElement> xml(state.createXml());
	copyXmlToBinary(*xml, destData);
}

void DifuserAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
	std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

	if (xmlState.get() != nullptr)
		if (xmlState->hasTagName(apvts.state.getType()))
			apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessorValueTreeState::ParameterLayout DifuserAudioProcessor::createParameterLayout()
{
	APVTS::ParameterLayout layout;

	using namespace juce;

	layout.add(std::make_unique<juce::AudioParameterFloat>(paramsNames[0], paramsNames[0], NormalisableRange<float>(  0.0f,  1.0f, 0.01f, 1.0f),   0.5f));
	layout.add(std::make_unique<juce::AudioParameterFloat>(paramsNames[1], paramsNames[1], NormalisableRange<float>(  2.0f,  8.0f, 0.01f, 1.0f),   4.0f));
	layout.add(std::make_unique<juce::AudioParameterFloat>(paramsNames[2], paramsNames[2], NormalisableRange<float>(-60.0f,  0.0f, 0.01f, 1.0f), -30.0f));
	layout.add(std::make_unique<juce::AudioParameterFloat>(paramsNames[3], paramsNames[3], NormalisableRange<float>(  0.0f,  1.0f, 0.01f, 1.0f),   0.5f));
	layout.add(std::make_unique<juce::AudioParameterFloat>(paramsNames[4], paramsNames[4], NormalisableRange<float>(-12.0f, 12.0f,  0.1f, 1.0f),   0.0f));

	return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DifuserAudioProcessor();
}
