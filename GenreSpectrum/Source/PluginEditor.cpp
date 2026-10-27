#include "PluginEditor.h"
#include "Presets.h"
#include <cmath>

static juce::String fmtFreq (double f)
{
    return f >= 1000.0 ? juce::String (f / 1000.0, f >= 10000.0 ? 0 : 1) + "k" : juce::String ((int) std::round (f));
}

SpectrumView::SpectrumView (GenreSpectrumProcessor& p) : proc (p)
{
    setOpaque (true);
    startTimerHz (30);
}

void SpectrumView::rebuildBands()
{
    const int div = curDiv;
    bLo.clear(); bHi.clear(); bFc.clear();
    const int n0 = (int) std::floor (div * std::log2 (25.0 / 1000.0));
    const int n1 = (int) std::ceil  (div * std::log2 (20000.0 / 1000.0));
    for (int n = n0; n <= n1; ++n)
    {
        const double fc = 1000.0 * std::pow (2.0, (double) n / div);
        bFc.push_back (fc);
        bLo.push_back (fc * std::pow (2.0, -0.5 / div));
        bHi.push_back (fc * std::pow (2.0,  0.5 / div));
    }
    bar.assign (bFc.size(), -120.0f);
    peak.assign (bFc.size(), -120.0f);
    avg.assign (bFc.size(), -120.0f);
}

float SpectrumView::refAt (double f, const std::vector<std::pair<float,float>>& c) const
{
    if (c.empty()) return 0.0f;
    if (f <= c.front().first) return c.front().second;
    if (f >= c.back().first)  return c.back().second;
    for (size_t i = 1; i < c.size(); ++i)
        if (f <= c[i].first)
        {
            const double t = std::log (f / c[i-1].first) / std::log ((double) c[i].first / c[i-1].first);
            return (float) (c[i-1].second + t * (c[i].second - c[i-1].second));
        }
    return c.back().second;
}

void SpectrumView::timerCallback()
{
    const int div = param ("bands") < 0.5f ? 3 : 6;
    const double sr = proc.sampleRateAtomic.load();
    if (div != curDiv || sr != curSr) { curDiv = div; curSr = sr; rebuildBands(); }

    static const float floors[] = { -40.0f, -50.0f, -60.0f, -80.0f, -100.0f };
    static const float slopes[] = { 0.0f, 3.0f, 4.5f };
    floorDb = floors[juce::jlimit (0, 4, (int) param ("range"))];
    slopeDb = slopes[juce::jlimit (0, 2, (int) param ("slope"))];

    if (param ("freeze") < 0.5f) update();
    repaint();
}

void SpectrumView::update()
{
    static const float rels[] = { 0.55f, 0.78f, 0.90f };
    const float rel = rels[juce::jlimit (0, 2, (int) param ("speed"))];
    const bool hold = param ("hold") > 0.5f;
    const double binHz = curSr / GenreSpectrumProcessor::fftSize;
    const int nb = (int) bFc.size();
    auto mag = [&] (int i) { return proc.magnitude[(size_t) juce::jlimit (0, GenreSpectrumProcessor::numBins - 1, i)].load(); };

    for (int b = 0; b < nb; ++b)
    {
        const int i0 = (int) std::ceil (bLo[(size_t) b] / binHz);
        const int i1 = (int) std::floor (bHi[(size_t) b] / binHz);

        const double pos = bFc[(size_t) b] / binHz;
        const int ip = (int) pos;
        const float a = juce::jmap ((float) (pos - ip), mag (ip), mag (ip + 1));
        float p = a * a;
        if (i1 >= i0)
        {
            double s = 0.0;
            for (int i = i0; i <= i1; ++i) { const float m = mag (i); s += (double) m * m; }
            p = juce::jmax (p, (float) (s / 1.5));
        }

        float db = 10.0f * std::log10 (p + 1.0e-12f) + slopeDb * (float) std::log2 (bFc[(size_t) b] / 1000.0);
        db = juce::jmax (db, -120.0f);

        auto i = (size_t) b;
        bar[i]  = db > bar[i] ? db : bar[i] * rel + db * (1.0f - rel);
        peak[i] = hold ? juce::jmax (peak[i], db) : juce::jmax (db, peak[i] - 0.35f);
        avg[i]  = avg[i] < -110.0f ? db : avg[i] * 0.97f + db * 0.03f;
    }

    // Alinear la referencia del preset al nivel real de la mezcla
    const auto& pr = getPresets()[(size_t) juce::jlimit (0, (int) getPresets().size() - 1, (int) param ("preset"))];
    if (! pr.curve.empty())
    {
        double sum = 0.0; int cnt = 0;
        for (int b = 0; b < nb; ++b)
            if (avg[(size_t) b] > floorDb + 6.0f)
            {
                sum += avg[(size_t) b] - (refAt (bFc[(size_t) b], pr.curve) + slopeDb * (float) std::log2 (bFc[(size_t) b] / 1000.0));
                ++cnt;
            }
        if (cnt > 3) refOffset = refOffset * 0.93f + (float) (sum / cnt) * 0.07f;
    }
}

void SpectrumView::mouseDown (const juce::MouseEvent&)
{
    std::fill (peak.begin(), peak.end(), -120.0f);   // reset de picos
    avg = bar;
}

void SpectrumView::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xFF124A82), 0, 0, juce::Colour (0xFF0A2C57), 0, (float) getHeight(), false));
    g.fillAll();

    auto plot = getLocalBounds().toFloat().withTrimmedLeft (44).withTrimmedRight (40).withTrimmedTop (24).withTrimmedBottom (24);
    auto xOf = [&] (double f) { return plot.getX() + plot.getWidth() * (float) (std::log (f / 20.0) / std::log (1000.0)); };
    auto yOf = [&] (float db) { return plot.getY() + plot.getHeight() * (-juce::jlimit (floorDb, 0.0f, db) / -floorDb); };

    const auto& presets = getPresets();
    const auto& pr = presets[(size_t) juce::jlimit (0, (int) presets.size() - 1, (int) param ("preset"))];

    // Zonas del preset
    g.setFont (11.0f);
    for (size_t i = 0; i < pr.zones.size(); ++i)
    {
        const auto& z = pr.zones[i];
        const float x0 = xOf (z.lo), x1 = xOf (z.hi);
        g.setColour (juce::Colours::white.withAlpha (i % 2 ? 0.07f : 0.03f));
        g.fillRect (juce::Rectangle<float> (x0, plot.getY(), x1 - x0, plot.getHeight()));
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.drawText (z.name, juce::Rectangle<float> (x0, plot.getY() + 2, x1 - x0, 14), juce::Justification::centred);
    }

    // Rejilla dB
    const int step = -floorDb <= 50.0f ? 5 : 10;
    g.setFont (11.0f);
    for (int db = 0; db >= (int) floorDb; db -= step)
    {
        const float y = yOf ((float) db);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        g.setColour (juce::Colours::white.withAlpha (0.6f));
        g.drawText (juce::String (db), juce::Rectangle<float> (0, y - 7, plot.getX() - 4, 14), juce::Justification::centredRight);
        g.drawText (juce::String (db), juce::Rectangle<float> (plot.getRight() + 4, y - 7, 34, 14), juce::Justification::centredLeft);
    }
    // Rejilla frecuencia
    for (double dec : { 10.0, 100.0, 1000.0, 10000.0 })
        for (int m = 1; m <= 9; ++m)
        {
            const double f = m * dec;
            if (f < 20.0 || f > 20000.0) continue;
            const float x = xOf (f);
            const bool major = (m == 1 || m == 2 || m == 5);
            g.setColour (juce::Colours::white.withAlpha (major ? 0.16f : 0.07f));
            g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            if (major)
            {
                g.setColour (juce::Colours::white.withAlpha (0.7f));
                g.drawText (fmtFreq (f), juce::Rectangle<float> (x - 22, plot.getBottom() + 4, 44, 14), juce::Justification::centred);
            }
        }

    // Referencia del preset (curva objetivo +-3 dB)
    if (! pr.curve.empty() && param ("ref") > 0.5f)
    {
        juce::Path mid;
        const int N = 80;
        for (int i = 0; i <= N; ++i)
        {
            const double f = 20.0 * std::pow (1000.0, (double) i / N);
            const float d = refAt (f, pr.curve) + refOffset + slopeDb * (float) std::log2 (f / 1000.0);
            const float x = xOf (f);
            if (i == 0) mid.startNewSubPath (x, yOf (d)); else mid.lineTo (x, yOf (d));
        }
        // polígono: ida por arriba, vuelta por abajo
        juce::Path poly;
        for (int i = 0; i <= N; ++i)
        {
            const double f = 20.0 * std::pow (1000.0, (double) i / N);
            const float d = refAt (f, pr.curve) + refOffset + slopeDb * (float) std::log2 (f / 1000.0);
            if (i == 0) poly.startNewSubPath (xOf (f), yOf (d + 3)); else poly.lineTo (xOf (f), yOf (d + 3));
        }
        for (int i = N; i >= 0; --i)
        {
            const double f = 20.0 * std::pow (1000.0, (double) i / N);
            const float d = refAt (f, pr.curve) + refOffset + slopeDb * (float) std::log2 (f / 1000.0);
            poly.lineTo (xOf (f), yOf (d - 3));
        }
        poly.closeSubPath();
        g.setColour (juce::Colour (0xFF47E08A).withAlpha (0.16f));
        g.fillPath (poly);

        juce::Path dashed;
        const float dashes[] = { 6.0f, 4.0f };
        juce::PathStrokeType (1.5f).createDashedStroke (dashed, mid, dashes, 2);
        g.setColour (juce::Colour (0xFF47E08A).withAlpha (0.9f));
        g.fillPath (dashed);
    }

    // Barras
    const int nb = (int) bFc.size();
    if (nb > 0)
    {
        g.saveState();
        g.reduceClipRegion (plot.toNearestInt());
        for (int b = 0; b < nb; ++b)
        {
            const float lvl = bar[(size_t) b];
            if (lvl <= floorDb) continue;
            const float x0 = xOf (bLo[(size_t) b]) + 1.0f, x1 = xOf (bHi[(size_t) b]) - 1.0f;
            const float y = yOf (lvl);
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xFFF4F8FF).withAlpha (0.95f), 0, y,
                                                     juce::Colour (0xFFB7CBE8).withAlpha (0.85f), 0, plot.getBottom(), false));
            g.fillRect (juce::Rectangle<float> (x0, y, juce::jmax (1.0f, x1 - x0), plot.getBottom() - y));
        }

        auto makeLine = [&] (const std::vector<float>& v)
        {
            juce::Path p;
            for (int b = 0; b < nb; ++b)
            {
                const float x = xOf (bFc[(size_t) b]), y = yOf (v[(size_t) b]);
                if (b == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            return p.createPathWithRoundedCorners (10.0f);
        };
        g.setColour (juce::Colour (0xFFFFFFFF).withAlpha (0.8f));
        g.strokePath (makeLine (avg), juce::PathStrokeType (1.6f));
        g.setColour (juce::Colour (0xFFE8B54A));
        g.strokePath (makeLine (peak), juce::PathStrokeType (1.8f));
        g.restoreState();
    }

    // Marco
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawRect (plot, 1.0f);

    // Titulo
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (juce::String ("Preset: ") + pr.name + "   |   Rango 0 a " + juce::String ((int) floorDb) + " dB",
                juce::Rectangle<float> (plot.getX(), 2, plot.getWidth(), 20), juce::Justification::centredLeft);
    g.setFont (11.0f);
    g.setColour (juce::Colours::white.withAlpha (0.55f));
    g.drawText ("Barras = nivel | Dorado = picos | Blanco = promedio | Verde = referencia | Clic = reset",
                juce::Rectangle<float> (plot.getX(), 2, plot.getWidth(), 20), juce::Justification::centredRight);

    // Lectura bajo el cursor
    if (plot.contains (mousePos) && nb > 0)
    {
        const double f = 20.0 * std::pow (1000.0, (mousePos.x - plot.getX()) / plot.getWidth());
        int best = 0;
        for (int b = 0; b < nb; ++b)
            if (std::abs (std::log (bFc[(size_t) b] / f)) < std::abs (std::log (bFc[(size_t) best] / f))) best = b;
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        auto r = juce::Rectangle<float> (mousePos.x + 10, mousePos.y - 22, 120, 18).constrainedWithin (plot);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (juce::Colours::white);
        g.drawText (fmtFreq (bFc[(size_t) best]) + " Hz   " + juce::String (bar[(size_t) best], 1) + " dB", r, juce::Justification::centred);
    }
}

// ---------------------------------------------------------------------------------------------

GenreSpectrumEditor::GenreSpectrumEditor (GenreSpectrumProcessor& p)
    : AudioProcessorEditor (&p), proc (p), view (p)
{
    setupCombo (preset, "preset");
    setupCombo (bands, "bands");
    setupCombo (range, "range");
    setupCombo (slope, "slope");
    setupCombo (speed, "speed");

    auto lab = [this] (juce::Label& l, const char* t) { l.setText (t, juce::dontSendNotification); l.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.8f)); l.setFont (juce::Font (juce::FontOptions (11.0f))); addAndMakeVisible (l); };
    lab (lPreset, "Preset"); lab (lBands, "Bandas"); lab (lRange, "Rango"); lab (lSlope, "Pendiente"); lab (lSpeed, "Velocidad");

    for (auto* b : { &hold, &freeze, &ref }) addAndMakeVisible (b);
    buttonAtt.push_back (std::make_unique<BA> (proc.apvts, "hold", hold));
    buttonAtt.push_back (std::make_unique<BA> (proc.apvts, "freeze", freeze));
    buttonAtt.push_back (std::make_unique<BA> (proc.apvts, "ref", ref));

    addAndMakeVisible (view);
    setResizable (true, true);
    setResizeLimits (640, 380, 2000, 1200);
    setSize (940, 540);
}

void GenreSpectrumEditor::setupCombo (juce::ComboBox& c, const char* id)
{
    if (auto* cp = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
        c.addItemList (cp->choices, 1);
    addAndMakeVisible (c);
    comboAtt.push_back (std::make_unique<CA> (proc.apvts, id, c));
}

void GenreSpectrumEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xFF081D3A)); }

void GenreSpectrumEditor::resized()
{
    auto r = getLocalBounds().reduced (8);
    auto top = r.removeFromTop (46);
    r.removeFromTop (6);
    view.setBounds (r);

    auto place = [&] (juce::Label& l, juce::ComboBox& c, int w)
    {
        auto col = top.removeFromLeft (w);
        l.setBounds (col.removeFromTop (16));
        c.setBounds (col.reduced (0, 2));
        top.removeFromLeft (8);
    };
    place (lPreset, preset, juce::jmax (170, getWidth() / 5));
    place (lBands, bands, 110);
    place (lRange, range, 110);
    place (lSlope, slope, 100);
    place (lSpeed, speed, 90);
    hold.setBounds (top.removeFromLeft (64));
    freeze.setBounds (top.removeFromLeft (70));
    ref.setBounds (top.removeFromLeft (56));
}
