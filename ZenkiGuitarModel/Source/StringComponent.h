#pragma once

#include <JuceHeader.h>

//==============================================================================
// Componente grafico che simula visivamente una corda vibrante.
// Utilizza una curva di Bezier quadratica animata tramite un timer interno (60Hz)
// per rappresentare l'oscillazione fisica in risposta a un "pizzico" (pluck).
class StringComponent final : public juce::Component, private juce::Timer
{
public:
    StringComponent(juce::Colour stringColour) : colour(stringColour)
    {
        // Disabilita l'intercettazione del mouse per delegarla interamente all'Editor principale
        setInterceptsMouseClicks(false, false);

        // Avvia il loop di animazione a 60 FPS
        startTimerHz(60);
    }

    // Innesca la vibrazione visiva calcolando l'ampiezza iniziale in base alla posizione. 
    // L'ampiezza massima si ottiene pizzicando esattamente al centro (0.5).
    void stringPlucked(float pluckPositionRelative)
    {
        amplitude = maxAmplitude * std::sin(pluckPositionRelative * juce::MathConstants<float>::pi);
        phase = juce::MathConstants<float>::pi;
    }

    // Esegue il rendering effettivo della corda sullo schermo
    void paint(juce::Graphics& g) override
    {
        g.setColour(colour);
        g.strokePath(generateStringPath(), juce::PathStrokeType(2.5f));
    }

    // Genera dinamicamente il tracciato vettoriale (curva quadratica) 
    // in base allo stato corrente di ampiezza e fase dell'oscillazione.
    juce::Path generateStringPath() const
    {
        float w = (float)getWidth();
        float y = (float)getHeight() / 2.0f;

        juce::Path p;
        p.startNewSubPath(0.0f, y);
        p.quadraticTo(w / 2.0f, y + std::sin(phase) * amplitude, w, y);

        return p;
    }

private:
    // Callback del timer: gestisce la fisica visiva dell'animazione (decadimento ed evoluzione della fase)
    // e richiede il ridisegno asincrono del componente.
    void timerCallback() override
    {
        // Decadimento esponenziale dell'oscillazione nel tempo (smorzamento visivo)
        amplitude *= 0.99f;

        // Avanzamento della fase proporzionale alla larghezza del componente
        float phaseStep = 400.0f / (float)juce::jmax(1, getWidth());
        phase += phaseStep;

        // Wrapping della fase (0-2PI) per mantenere la stabilità numerica nel tempo
        if (phase >= juce::MathConstants<float>::twoPi)
            phase -= juce::MathConstants<float>::twoPi;

        repaint();
    }

#pragma region Variabili di Stato Grafico
    juce::Colour colour;
    float amplitude = 0.0f;
    const float maxAmplitude = 12.0f;
    float phase = 0.0f;
#pragma endregion

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StringComponent)
};