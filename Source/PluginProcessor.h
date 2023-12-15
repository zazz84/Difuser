/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
class CircularBuffer
{
public:
	CircularBuffer();

	void Init(int size);
	void WriteSample(float sample)
	{
		m_buffer.setSample(0, m_head, sample);
		if (++m_head >= m_size)
			m_head = 0;
	}
	float Read() const
	{
		return m_buffer.getSample(0, m_head);
	}
	float ReadDelay(float sample);
	float ReadFactor(float factor);
	void Clear();

protected:
	juce::AudioBuffer<float> m_buffer;
	int m_head = 0;
	int m_size = 0;
};

//==============================================================================
class DelayLineDifuser
{
	static const int N_DELAY_LINES = 4;
	static const int N_STAGES = 8;
public:
	DelayLineDifuser();

	void Init(float delayFactor, int sampleRate);
	float ProcessSample(float inSample, float factor, int density);
	void Clear();

private:
	CircularBuffer m_buffer[N_STAGES][N_DELAY_LINES];
};

//==============================================================================
class EnvelopeFollower
{
public:
	EnvelopeFollower();

	void Init(int sampleRate);
	void SetCoef(float attackTime, float releaseTime);
	float process(float in);

protected:
	int  m_SampleRate = 0;
	float m_Envelope = 0.0f;
	float m_AttackCoef = 0.0f;
	float m_ReleaseCoef = 0.0f;
};

//==============================================================================
/**
*/
class DifuserAudioProcessor  : public juce::AudioProcessor
                            #if JucePlugin_Enable_ARA
                             , public juce::AudioProcessorARAExtension
                            #endif
{
public:
    //==============================================================================
    DifuserAudioProcessor();
    ~DifuserAudioProcessor() override;

	static const std::string paramsNames[];

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

	using APVTS = juce::AudioProcessorValueTreeState;
	static APVTS::ParameterLayout createParameterLayout();

	APVTS apvts{ *this, nullptr, "Parameters", createParameterLayout() };

private:
    //==============================================================================
	std::atomic<float>* difusionLenghtParameter = nullptr;
	std::atomic<float>* densityParameter = nullptr;
	std::atomic<float>* thresholdParameter = nullptr;
	std::atomic<float>* mixParameter = nullptr;
	std::atomic<float>* volumeParameter = nullptr;

	DelayLineDifuser m_delayLineDifuser[2] = {};
	EnvelopeFollower m_envelopeFollower[2] = {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DifuserAudioProcessor)
};
