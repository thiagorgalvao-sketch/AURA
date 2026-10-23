#pragma once

// Geometria do painel do AURA Bass, em unidades de interface (1 unidade = 1 px lógico).
// É usada tanto pelo renderizador das imagens (Tools/AuraRender) quanto pelo editor,
// garantindo que knobs e footswitches fiquem exatamente sobre o que foi renderizado.
namespace AuraLayout
{
    inline constexpr int width  = 980;
    inline constexpr int height = 470;
    inline constexpr int renderScale = 2;            // imagens em 2x (telas Retina)

    inline constexpr float chassisX = 8.0f, chassisY = 8.0f;
    inline constexpr float chassisW = 964.0f, chassisH = 454.0f;
    inline constexpr float chassisCorner = 24.0f;

    // Três tamanhos de knob (cada um tem sua própria sequência de quadros)
    struct KnobClass { float radius; float height; };
    inline constexpr KnobClass knobClasses[3] = {
        { 62.0f, 24.0f },   // grande  (Drive, Level)
        { 46.0f, 21.0f },   // médio   (Blend)
        { 36.0f, 19.0f },   // pequeno (Low, Mid, High, Presence)
    };
    inline constexpr int knobFrames = 81;
    inline constexpr float knobMinAngleDeg = -135.0f, knobMaxAngleDeg = 135.0f;
    inline constexpr float knobFrameMargin = 3.0f;   // borda extra em volta do knob em cada quadro

    struct Knob { const char* paramId; const char* label; float cx, cy; int cls; };
    inline constexpr Knob knobs[7] = {
        { "drive",    "DRIVE",    143.0f, 142.0f, 0 },
        { "blend",    "BLEND",    282.0f, 142.0f, 1 },
        { "low",      "LOW",      411.5f, 142.0f, 2 },
        { "mid",      "MID",      510.5f, 142.0f, 2 },
        { "high",     "HIGH",     609.5f, 142.0f, 2 },
        { "presence", "PRESENCE", 708.5f, 142.0f, 2 },
        { "level",    "LEVEL",    861.0f, 142.0f, 0 },
    };
    inline constexpr float knobLabelY = 246.0f;

    struct Group { const char* title; float x1, x2; };
    inline constexpr Group groups[3] = {
        { "OVERDRIVE", 44.0f, 334.0f },
        { "TONE",      362.0f, 758.0f },
        { "OUTPUT",    786.0f, 936.0f },
    };
    inline constexpr float groupTitleY = 41.0f;
    inline constexpr float separatorY = 270.0f;

    struct Switch { const char* paramId; const char* label; float cx, cy, ledX, ledY; };
    inline constexpr Switch switches[2] = {
        { "driveOn", "DRIVE", 205.0f, 362.0f, 205.0f, 293.0f },
        { "cab",     "CAB",   775.0f, 362.0f, 775.0f, 293.0f },
    };
    inline constexpr float switchRadius = 44.0f;     // porca sextavada (até os cantos)
    inline constexpr float switchLabelY = 426.0f;
    inline constexpr float ledRadius = 6.0f;

    // Recortes (em unidades) das variações pré-renderizadas
    inline constexpr float switchCropHalf = 56.0f;   // footswitch pressionado
    inline constexpr float ledCropHalf    = 26.0f;   // LED aceso (com brilho no metal)

    inline constexpr float wordmarkX = 490.0f, wordmarkY = 340.0f, taglineY = 380.0f;

    // Câmera levemente inclinada (projeção oblíqua): um ponto a altura z aparece
    // z * obliqueK unidades mais acima na tela. Por isso as laterais dos knobs aparecem.
    inline constexpr float obliqueK = 0.45f;

    // (cx, cy) de cada knob é o centro do TOPO na tela; a base fica mais abaixo
    constexpr float knobBaseY (const Knob& k) { return k.cy + knobClasses[k.cls].height * obliqueK; }

    // Retângulo (na tela) de cada quadro da sequência do knob
    struct Rect { float x, y, w, h; };
    constexpr Rect knobFrameRect (const Knob& k)
    {
        const float r = knobClasses[k.cls].radius + knobFrameMargin;
        return { k.cx - r, k.cy - r, 2.0f * r, 2.0f * r + knobClasses[k.cls].height * obliqueK };
    }
}
