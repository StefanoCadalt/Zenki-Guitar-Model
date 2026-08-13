#pragma once

#include <JuceHeader.h>

//==============================================================================
// Definisce il tema grafico personalizzato per i componenti UI, 
// sovrascrivendo i metodi di disegno standard di JUCE per manopole e bottoni.
class KnobStyle : public juce::LookAndFeel_V4
{
public:
#pragma region Rotary Slider
    // Disegna l'aspetto grafico delle manopole rotative (stile vettoriale minimale e luminoso).
    void drawRotarySlider(juce::Graphics& g,
        int x, int y, int width, int height,
        float sliderPos,
        float rotaryStartAngle,
        float rotaryEndAngle,
        juce::Slider&) override
    {
        // Calcola il centro e il raggio utile, lasciando un margine di respiro
        auto radius = juce::jmin(width, height) / 2.0f - 10.0f;
        auto centreX = x + width * 0.5f;
        auto centreY = y + height * 0.5f;

        // Mappa la posizione lineare dello slider (0.0 - 1.0) sull'angolo di rotazione corrispondente
        auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        juce::Colour cianoGhiaccio(0xFF00D4FF);

        // Disegno dello sfondo interno (nucleo scuro centrale)
        g.setColour(juce::Colour(0xFF08080A));
        g.fillEllipse(centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f);

        // Disegno dell'anello esterno di delimitazione
        g.setColour(cianoGhiaccio.withAlpha(0.3f));
        g.drawEllipse(centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f, 2.5f);

        // Generazione del percorso vettoriale per la linea dell'indicatore (il "LED" della manopola)
        juce::Path p;
        auto pointerLength = radius * 0.85f;
        auto pointerThickness = 3.0f;
        p.addRectangle(-pointerThickness * 0.5f, -radius, pointerThickness, pointerLength);

        // Applica la rotazione calcolata all'indicatore e lo posiziona sul centro
        g.setColour(cianoGhiaccio);
        g.fillPath(p, juce::AffineTransform::rotation(angle).translated(centreX, centreY));
    }
#pragma endregion

#pragma region Button Background
    // Disegna lo sfondo dei bottoni, applicando variazioni stilistiche in base allo stato (Toggle ON/OFF).
    void drawButtonBackground(juce::Graphics& g,
        juce::Button& button,
        const juce::Colour& backgroundColour,
        bool shouldDrawButtonAsHighlighted,
        bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        float cornerSize = 1.0f;

        // Valutazione dello stato operativo del bottone per l'assegnazione dei colori
        if (button.getToggleState())
        {
            // Stato ON: Sfondo semitrasparente e bordo acceso
            g.setColour(juce::Colour(0xFF00D4FF).withAlpha(0.15f));
            g.fillRoundedRectangle(bounds, cornerSize);

            g.setColour(juce::Colour(0xFF00D4FF));
            g.drawRoundedRectangle(bounds, cornerSize, 1.5f);
        }
        else
        {
            // Stato OFF: Sfondo scuro e bordo tenue per uniformarsi ai pannelli hardware circostanti
            g.setColour(juce::Colour(0xFF08080A));
            g.fillRoundedRectangle(bounds, cornerSize);

            g.setColour(juce::Colour(0xFF20D065).withAlpha(0.2f));
            g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
        }
    }
#pragma endregion
};