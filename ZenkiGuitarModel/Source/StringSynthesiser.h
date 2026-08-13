#pragma once

#include <JuceHeader.h>

//==============================================================================
// Implementazione del modello fisico di una singola corda vibrante 
// basata sull'algoritmo di Karplus-Strong con estensione Jaffe-Smith 
// per l'intonazione fine tramite ritardo frazionario (Allpass filter).
class StringSynthesiser
{
public:
    // Inizializza il sintetizzatore calcolando la lunghezza massima del buffer 
    // di ritardo in base a una frequenza minima (es. 20Hz) per evitare riallocazioni.
    StringSynthesiser(double sampleRate, double frequencyInHz, float hardness)
        : fs(sampleRate), currentHardness(hardness)
    {
        doPluckForNextBuffer.set(false);
        maxDelayLength = (size_t)juce::roundToInt(sampleRate / 20.0);
        delayLine.resize(maxDelayLength, 0.0f);
        excitationSample.resize(maxDelayLength, 0.0f);
        setFrequency(frequencyInHz);
    }

    void SetHardness(float h) { currentHardness = juce::jlimit(0.01f, 1.0f, h); }
    void SetDamping(float d) { currentDamping = juce::jlimit(0.0f, 1.0f, d); }
    void SetSustain(float s) { currentSustain = juce::jlimit(0.0f, 1.0f, s); }

    // Registra la plettrata in modo thread-safe. 
    // Calcola l'ampiezza dell'eccitazione basata sulla posizione del pizzico (0.0 - 1.0).
    void stringPlucked(float pluckPosition)
    {
        if (doPluckForNextBuffer.compareAndSetBool(1, 0))
            amplitude = std::sin(juce::MathConstants<float>::pi * pluckPosition);
    }

    // Core DSP loop: genera i campioni audio della corda simulata 
    // e li somma (mix) direttamente nel buffer di uscita fornito.
    void generateAndAddData(float* outBuffer, int numSamples)
    {
        // Se la corda è stata appena pizzicata, inietta il burst di rumore nel buffer
        if (doPluckForNextBuffer.compareAndSetBool(0, 1))
            exciteInternalBuffer();

        float dampCoeff = currentDamping * 0.5f;
        float feedbackGain = 0.9f + currentSustain * 0.099f;

        for (int i = 0; i < numSamples; ++i)
        {
            auto nextPos = (pos + 1) % currentDelayLength;

            // 1. Filtro di damping (passa-basso del primo ordine)
            float filtered = delayLine[pos] * (1.0f - dampCoeff) + delayLine[nextPos] * dampCoeff;

            // 2. Allpass filter per fractional delay (intonazione fine Jaffe-Smith)
            float allpassOut = allpassCoeff * filtered + allpassInputPrec - allpassCoeff * allpassOutputPrec;
            allpassInputPrec = filtered;
            allpassOutputPrec = allpassOut;

            // 3. Feedback gain e scrittura nel delay buffer
            delayLine[nextPos] = allpassOut * feedbackGain;

            outBuffer[i] += delayLine[pos];
            pos = nextPos;
        }
    }

    // Calcola la lunghezza esatta del ritardo (in campioni) necessaria per ottenere 
    // la frequenza desiderata. Divide il valore in una parte intera (buffer size) 
    // e una parte frazionaria (gestita dal filtro Allpass).
    void setFrequency(double newFrequencyInHz)
    {
        double exactDelay = fs / newFrequencyInHz;
        size_t N = (size_t)std::floor(exactDelay);
        float frac = (float)(exactDelay - (double)N);

        currentDelayLength = juce::jlimit((size_t)2, maxDelayLength, N);

        // Coefficiente allpass per interpolazione frazionaria (Jaffe-Smith)
        allpassCoeff = (1.0f - frac) / (1.0f + frac);

        pos = 0;
        allpassInputPrec = 0.0f;
        allpassOutputPrec = 0.0f;

        generateExcitation();
    }

private:
#pragma region DSP Internals
    // Genera il burst di rumore (impulso di eccitazione) e applica il low-pass shaping in base all'hardness
    void generateExcitation()
    {
        float lastSample = 0.0f;
        float maxVal = 0.0f;

        for (size_t i = 0; i < currentDelayLength; ++i)
        {
            // Generazione white noise [-1.0, 1.0]
            float noise = (juce::Random::getSystemRandom().nextFloat() * 2.0f) - 1.0f;

            // Shaping pass-basso (Hardness: 1.0 = plettro rigido -> 0.0 = dito morbido)
            float shaped = noise * currentHardness + lastSample * (1.0f - currentHardness);
            excitationSample[i] = shaped;

            lastSample = shaped;
            maxVal = std::max(maxVal, std::abs(shaped));
        }

        // Normalizzazione del picco dell'impulso
        if (maxVal > 0.0f)
        {
            for (size_t i = 0; i < currentDelayLength; ++i)
                excitationSample[i] /= maxVal;
        }
    }

    // Moltiplica il campione di eccitazione normalizzato per l'ampiezza del pizzico 
    // e lo scrive nella delay line per dare inizio all'oscillazione.
    void exciteInternalBuffer()
    {
        generateExcitation();
        for (size_t i = 0; i < currentDelayLength; ++i)
            delayLine[i] = excitationSample[i] * (float)amplitude;
    }
#pragma endregion

#pragma region Variabili di Stato
    double fs;
    size_t maxDelayLength;
    size_t currentDelayLength = 0;

    const double decay = 0.998;
    double amplitude = 0.0;

    float currentHardness = 0.5f;
    float currentDamping = 0.5f;
    float currentSustain = 0.8f;

    float allpassCoeff = 0.0f;
    float allpassInputPrec = 0.0f;
    float allpassOutputPrec = 0.0f;

    juce::Atomic<int> doPluckForNextBuffer;
    std::vector<float> excitationSample, delayLine;
    size_t pos = 0;
#pragma endregion

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StringSynthesiser)
};