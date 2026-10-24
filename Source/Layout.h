#pragma once

// Geometria do painel do AURA Bass em unidades de interface (1 unidade = 1 px lógico).
// As imagens em Resources/ui são 2x. As coordenadas vêm da arte de referência
// (Resources/mockup, recortada em Tools/build_ui_assets.py): unidade = (px - recorte) / 2.
namespace AuraLayout
{
    inline constexpr int width  = 726;
    inline constexpr int height = 339;

    struct Knob { const char* paramId; float cx, cy; float valueX; };
    inline constexpr Knob knobs[7] = {
        { "level",     71.50f, 192.50f,  74.5f },
        { "drive",    159.25f, 194.50f, 162.0f },
        { "low",      247.75f, 192.50f, 249.0f },
        { "mid",      336.00f, 192.50f, 336.5f },
        { "high",     424.50f, 192.50f, 423.5f },
        { "presence", 513.50f, 192.75f, 510.5f },
        { "blend",    601.00f, 192.25f, 598.0f },
    };
    inline constexpr float knobHitRadius = 34.0f;
    inline constexpr float knobMinAngleDeg = -135.0f, knobMaxAngleDeg = 135.0f;
    inline constexpr float pointerHalf = 24.0f;      // quadro do indicador (pointer.png) = 48x48
    inline constexpr float valueY = 252.0f;          // centro vertical dos valores
    inline constexpr float valueFontSize = 14.0f;

    // Footswitch do DRIVE + LED azul + rótulo
    inline constexpr float driveSwitchX = 655.5f, driveSwitchY = 298.0f;   // canto inferior direito
    inline constexpr float driveSwitchDownHalf = 24.0f;   // sobreposição "pressionado"
    inline constexpr float driveLedX = 616.0f, driveLedY = 298.0f, driveLedHalf = 11.0f;
    struct Rect { float x, y, w, h; };
    inline constexpr Rect driveHit { 564.0f, 274.0f, 116.0f, 48.0f };

    // CAB (rótulo, LED laranja e chave alavanca)
    inline constexpr float cabLedX = 671.35f, cabLedY = 158.65f;
    inline constexpr float cabToggleX = 671.5f, cabToggleY = 195.0f;
    inline constexpr Rect cabHit { 648.0f, 128.0f, 48.0f, 94.0f };

    // Sobreposições (meio lado em unidades)
    inline constexpr float ledHalf = 16.0f;
    inline constexpr float toggleHalf = 25.0f;
}
