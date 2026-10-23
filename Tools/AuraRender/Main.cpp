// AuraRender — gera as imagens fotorrealistas da interface do AURA Bass.
//
// Renderizador físico simples: a cena (chassi, knobs, footswitches, parafusos, LEDs)
// é descrita como um mapa de alturas visto de cima. Cada pixel é iluminado por um
// ambiente de estúdio (softboxes e luzes de recorte) com integração de Monte Carlo:
//   - metais com microfacetas GGX anisotrópicas (alumínio escovado e torneado),
//   - Fresnel de Schlick com F0 do alumínio/cromo, sombreamento de Smith,
//   - sombras e oclusão pelo mapa de horizonte,
//   - tinta serigrafada como dielétrico fosco.
//
// Uso:  AuraRender <pasta de saída> [amostras]
// Saída: background.jpg, knob_<n>.jpg + knob_<n>_mask.png, switch_<i>_down.jpg, led_<i>_on.jpg

#include <juce_graphics/juce_graphics.h>
#include "../../Source/Layout.h"
#include <atomic>
#include <cmath>
#include <functional>
#include <thread>
#include <vector>

using namespace juce;
namespace L = AuraLayout;

//==============================================================================
// Vetores
//==============================================================================
struct V3
{
    float x = 0, y = 0, z = 0;
    V3() = default;
    V3 (float a, float b, float c) : x (a), y (b), z (c) {}
    V3 operator+ (V3 o) const { return { x + o.x, y + o.y, z + o.z }; }
    V3 operator- (V3 o) const { return { x - o.x, y - o.y, z - o.z }; }
    V3 operator* (float s) const { return { x * s, y * s, z * s }; }
    V3 operator* (V3 o) const { return { x * o.x, y * o.y, z * o.z }; }
    V3& operator+= (V3 o) { x += o.x; y += o.y; z += o.z; return *this; }
};
static float dot (V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static V3 cross (V3 a, V3 b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
static V3 normalize (V3 a) { const float l = std::sqrt (dot (a, a)); return l > 0 ? a * (1.0f / l) : V3 (0, 0, 1); }
static float clamp01 (float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
static float smoothstep (float a, float b, float v) { const float t = clamp01 ((v - a) / (b - a)); return t * t * (3 - 2 * t); }
static float lerpf (float a, float b, float t) { return a + (b - a) * t; }
constexpr float kPi = 3.14159265358979f;

// Gerador rápido por pixel
struct Rng
{
    uint32_t s;
    explicit Rng (uint32_t seed) : s (seed * 747796405u + 2891336453u) {}
    float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s & 0xffffff) / 16777216.0f; }
};

// Ruído de valor 2D
static float hash2 (int x, int y)
{
    uint32_t h = (uint32_t) x * 374761393u + (uint32_t) y * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return (float) ((h ^ (h >> 16)) & 0xffff) / 65535.0f;
}
static float vnoise (float x, float y)
{
    const int xi = (int) std::floor (x), yi = (int) std::floor (y);
    const float fx = x - (float) xi, fy = y - (float) yi;
    const float ux = fx * fx * (3 - 2 * fx), uy = fy * fy * (3 - 2 * fy);
    const float a = hash2 (xi, yi), b = hash2 (xi + 1, yi), c = hash2 (xi, yi + 1), d = hash2 (xi + 1, yi + 1);
    return lerpf (lerpf (a, b, ux), lerpf (c, d, ux), uy);
}

//==============================================================================
// Ambiente de estúdio. Direções: x = direita, y = para baixo na tela, z = para cima (para a câmera)
//==============================================================================
static V3 environment (V3 d)
{
    if (d.z <= 0.0f)
        return V3 (0.018f, 0.018f, 0.02f);

    const float el = std::asin (std::min (1.0f, d.z)) * 180.0f / kPi;      // elevação
    const float az = std::atan2 (-d.y, d.x) * 180.0f / kPi;                 // 90 = topo da imagem

    auto azDiff = [] (float a, float b) { float x = std::fmod (a - b + 540.0f, 360.0f) - 180.0f; return x; };

    // Base: estúdio cinza com o teto mais claro
    float v = 0.11f + 0.08f * d.z;
    V3 col (v, v, v * 1.03f);

    // Softbox principal, grande, acima e um pouco atrás (topo da imagem)
    auto box = [&] (float cAz, float cEl, float hAz, float hEl, float soft, float intensity, V3 tint)
    {
        const float dx = azDiff (az, cAz) * std::cos (el * kPi / 180.0f);
        const float dy = el - cEl;
        const float m = (1.0f - smoothstep (hAz - soft, hAz + soft, std::abs (dx)))
                      * (1.0f - smoothstep (hEl - soft, hEl + soft, std::abs (dy)));
        col += tint * (intensity * m);
    };

    // Perto do zênite o azimute perde sentido: trata a softbox principal em coordenadas cartesianas
    {
        const float cx = 0.0f, cy = -0.36f;           // centro deslocado para o topo
        const float px = d.x, py = d.y;
        const float mx = 1.0f - smoothstep (0.30f, 0.62f, std::abs (px - cx));
        const float my = 1.0f - smoothstep (0.06f, 0.34f, std::abs (py - cy));
        col += V3 (1.0f, 0.99f, 0.97f) * (0.80f * mx * my);
        // Faixa mais forte no meio da softbox (difusor)
        const float core = (1.0f - smoothstep (0.15f, 0.35f, std::abs (px - cx))) * (1.0f - smoothstep (0.08f, 0.22f, std::abs (py - cy)));
        col += V3 (1.0f, 0.98f, 0.95f) * (0.45f * core);
    }

    box (-90.0f, 34.0f, 55.0f, 12.0f, 6.0f, 0.55f, V3 (0.95f, 0.97f, 1.0f));  // rebatedor frontal (baixo da imagem)
    box (0.0f, 32.0f, 5.0f, 24.0f, 3.0f, 1.6f, V3 (1.0f, 1.0f, 1.0f));        // strip à direita
    box (180.0f, 28.0f, 4.0f, 20.0f, 3.0f, 0.9f, V3 (1.0f, 0.97f, 0.93f));    // strip à esquerda (quente)
    box (90.0f, 22.0f, 40.0f, 9.0f, 5.0f, 0.45f, V3 (1.0f, 1.0f, 1.0f));      // painel claro ao fundo
    box (-90.0f, 8.0f, 70.0f, 6.0f, 4.0f, 0.25f, V3 (1.0f, 1.0f, 1.0f));      // reflexo da mesa
    return col;
}

//==============================================================================
// Cena
//==============================================================================
enum Mat { M_FLOOR, M_BRUSHED, M_SPUN, M_POLISH, M_KNURL, M_GROOVE, M_CHROME, M_SATIN, M_SLOT, M_LED };

struct Surf
{
    float h = 0;
    Mat mat = M_FLOOR;
    float paint = 0;          // 0..1 cobertura da serigrafia
    float cx = 0, cy = 0;     // centro do objeto (para tangentes radiais/circulares)
    int led = -1;             // índice do LED
    float cavity = 0;         // 0..1 (vales do serrilhado)
    V3 wallN;                 // normal da parede lateral do objeto (se houver)
};

struct Scene
{
    bool showKnobs = true;
    float knobAngle[7] {};    // radianos (0 = para cima, sentido horário)
    int onlyKnob = -1;        // modo "quadro de knob": só esse knob, sobre plano
    float onlyKnobX = 0, onlyKnobY = 0;
    bool pressed[2] { false, false };
    bool ledOn[2] { false, false };
    const std::vector<uint8_t>* paintMask = nullptr;   // maskScale px por unidade
    int maskW = 0, maskH = 0, maskScale = 4;
};

static float roundedRectSdf (float x, float y, float rx, float ry, float rw, float rh, float r)
{
    const float cx = rx + rw * 0.5f, cy = ry + rh * 0.5f;
    const float qx = std::abs (x - cx) - (rw * 0.5f - r);
    const float qy = std::abs (y - cy) - (rh * 0.5f - r);
    const float ox = std::max (qx, 0.0f), oy = std::max (qy, 0.0f);
    return std::sqrt (ox * ox + oy * oy) + std::min (std::max (qx, qy), 0.0f) - r;
}

static float hexSdf (float x, float y, float r)   // hexágono com vértices em ±x (r = até o canto)
{
    const float k1 = 0.8660254f, k2 = 0.5f;
    x = std::abs (x); y = std::abs (y);
    const float apothem = r * k1;
    // distância ao lado: max(projeções nas normais dos lados)
    return std::max (y - apothem, x * k1 + y * k2 - apothem);
}

static float sampleMask (const Scene& sc, float x, float y)
{
    if (sc.paintMask == nullptr) return 0.0f;
    const float fx = x * (float) sc.maskScale - 0.5f, fy = y * (float) sc.maskScale - 0.5f;
    const int x0 = (int) std::floor (fx), y0 = (int) std::floor (fy);
    const float tx = fx - (float) x0, ty = fy - (float) y0;
    auto px = [&] (int xx, int yy) -> float
    {
        if (xx < 0 || yy < 0 || xx >= sc.maskW || yy >= sc.maskH) return 0.0f;
        return (float) (*sc.paintMask)[(size_t) yy * (size_t) sc.maskW + (size_t) xx] / 255.0f;
    };
    return lerpf (lerpf (px (x0, y0), px (x0 + 1, y0), tx), lerpf (px (x0, y0 + 1), px (x0 + 1, y0 + 1), tx), ty);
}

// Knob: alturas e materiais relativos ao centro
static bool knobSurface (float x, float y, float cx, float cy, const L::KnobClass& kc, float angle, Surf& s)
{
    const float dx = x - cx, dy = y - cy;
    const float r = std::sqrt (dx * dx + dy * dy);
    const float R = kc.radius, H = kc.height;
    if (r > R) return false;

    s.cx = cx; s.cy = cy;
    const float rTop = 0.76f * R, rCh = 0.82f * R;

    if (r <= rTop)
    {
        s.h = H + 0.35f * (1.0f - (r / rTop) * (r / rTop));    // tampa levemente abaulada
        s.mat = M_SPUN;

        // Ranhura do indicador
        const float px = std::sin (angle), py = -std::cos (angle);
        const float t = dx * px + dy * py;                       // ao longo
        const float n = std::abs (dx * py - dy * px);           // distância perpendicular
        const float w = std::max (1.3f, R * 0.026f);
        const float t0 = R * 0.20f, t1 = R * 0.70f;
        float dist = n;
        if (t < t0) dist = std::sqrt (n * n + (t0 - t) * (t0 - t));
        else if (t > t1) dist = std::sqrt (n * n + (t - t1) * (t - t1));
        if (dist < w)
        {
            const float u = dist / w;
            s.h -= 0.9f * std::sqrt (std::max (0.0f, 1.0f - u * u));
            if (u < 0.8f) s.mat = M_GROOVE;
        }
        return true;
    }
    if (r <= rCh)
    {
        const float t = (r - rTop) / (rCh - rTop);
        s.h = H - 1.6f * t * t;                                   // chanfro polido
        s.mat = M_POLISH;
        return true;
    }

    // Saia serrilhada
    const float t = (r - rCh) / (R - rCh);
    const float drop = 7.0f * std::pow (t, 1.6f);
    const int teeth = (int) std::round (R * 1.45f);
    const float phi = std::atan2 (dy, dx) - angle;
    float tri = std::fmod (phi / (2.0f * kPi) * (float) teeth + 1000.0f, 1.0f);
    tri = 1.0f - std::abs (tri * 2.0f - 1.0f);                     // 0..1 triangular
    const float ridge = (tri - 0.5f) * 1.1f * smoothstep (0.0f, 0.12f, t) * (1.0f - smoothstep (0.9f, 1.0f, t));
    s.h = H - 1.6f - drop + ridge;
    s.mat = M_KNURL;
    s.cavity = clamp01 (0.5f - (tri - 0.5f)) * smoothstep (0.0f, 0.2f, t);

    // Lateral serrilhada: normal radial inclinada pelos dentes
    {
        const float fr = std::fmod (phi / (2.0f * kPi) * (float) teeth + 1000.0f, 1.0f);
        const float sgn = fr < 0.5f ? 1.0f : -1.0f;
        const V3 radial (dx / r, dy / r, 0.0f), tang (-dy / r, dx / r, 0.0f);
        s.wallN = normalize (radial + tang * (0.55f * sgn));
        s.cavity = std::max (s.cavity, 0.0f);
    }
    return true;
}

static float evalScene (const Scene& sc, float x, float y, Surf& s)
{
    s = Surf();

    // Modo quadro de knob: plano neutro + um knob
    if (sc.onlyKnob >= 0)
    {
        const auto& k = L::knobs[sc.onlyKnob];
        s.h = 0; s.mat = M_BRUSHED;
        knobSurface (x, y, sc.onlyKnobX, L::knobBaseY (k), L::knobClasses[k.cls], sc.knobAngle[sc.onlyKnob], s);
        return s.h;
    }

    // Chassi com borda arredondada
    const float d = roundedRectSdf (x, y, L::chassisX, L::chassisY, L::chassisW, L::chassisH, L::chassisCorner);
    const float rb = 6.0f;
    if (d > 0.0f)
    {
        s.h = -8.0f; s.mat = M_FLOOR;
        return s.h;
    }
    s.mat = M_BRUSHED;
    if (d > -rb)
    {
        const float t = rb + d;               // 0 no início do arredondado, rb na borda
        s.h = std::sqrt (std::max (0.0f, rb * rb - t * t)) - rb;
        s.h = std::max (s.h, -rb) * 1.0f;
    }
    else
    {
        s.h = 0.0f;
        s.paint = sampleMask (sc, x, y);
    }

    // Parafusos
    const float inset = 16.0f;
    const float screws[4][3] = { { L::chassisX + inset, L::chassisY + inset, 0.4f },
                                 { L::chassisX + L::chassisW - inset, L::chassisY + inset, 1.1f },
                                 { L::chassisX + inset, L::chassisY + L::chassisH - inset, 0.9f },
                                 { L::chassisX + L::chassisW - inset, L::chassisY + L::chassisH - inset, 0.2f } };
    for (auto& sw : screws)
    {
        const float dx = x - sw[0], dy = y - sw[1];
        const float r2 = dx * dx + dy * dy, R = 5.5f;
        if (r2 < R * R)
        {
            s.paint = 0; s.cx = sw[0]; s.cy = sw[1];
            s.h = 2.0f * std::sqrt (1.0f - r2 / (R * R)) + 0.4f;
            s.mat = M_CHROME;
            const float ca = std::cos (sw[2]), sa = std::sin (sw[2]);
            const float u = dx * ca + dy * sa, v = -dx * sa + dy * ca;
            if ((std::abs (u) < 0.65f && std::abs (v) < 3.6f) || (std::abs (v) < 0.65f && std::abs (u) < 3.6f))
            {
                s.h -= 1.4f; s.mat = M_SLOT;
            }
            return s.h;
        }
    }

    // Knobs
    if (sc.showKnobs)
        for (int i = 0; i < 7; ++i)
        {
            const auto& k = L::knobs[i];
            Surf ks;
            if (knobSurface (x, y, k.cx, L::knobBaseY (k), L::knobClasses[k.cls], sc.knobAngle[i], ks))
            {
                s = ks;
                return s.h;
            }
        }

    // Footswitches
    for (int i = 0; i < 2; ++i)
    {
        const auto& sw = L::switches[i];
        const float dx = x - sw.cx, dy = y - sw.cy;
        const float r = std::sqrt (dx * dx + dy * dy);
        const float R = L::switchRadius;
        const float hexD = hexSdf (dx, dy, R);
        if (hexD < 0.0f)
        {
            s.paint = 0; s.cx = sw.cx; s.cy = sw.cy;
            const float rW = 0.80f * R, rD = 0.56f * R;
            s.wallN = V3 (dx / (r + 1e-4f), dy / (r + 1e-4f), 0.0f);
            if (r < rD)
            {
                const float top = sc.pressed[i] ? 14.0f : 17.0f;
                const float u = r / rD;
                s.h = top + 7.0f * std::sqrt (std::max (0.0f, 1.0f - u * u)) * 0.6f;
                s.mat = M_CHROME;
            }
            else if (r < rD + 1.2f)
            {
                s.h = 11.0f; s.mat = M_SLOT;          // folga escura entre botão e arruela
            }
            else if (r < rW)
            {
                const float u = (r - rD) / (rW - rD);
                s.h = 12.0f - 2.5f * u * u;            // arruela
                s.mat = M_CHROME;
            }
            else
            {
                const float bev = 1.8f;
                s.h = 8.0f - (hexD > -bev ? (bev + hexD) * (bev + hexD) / bev : 0.0f);
                s.mat = M_SATIN;                        // porca
                const float a = std::atan2 (dy, dx);
                const float sector = std::round ((a - kPi / 6.0f) / (kPi / 3.0f));
                const float na = kPi / 6.0f + sector * (kPi / 3.0f);
                s.wallN = V3 (std::cos (na), std::sin (na), 0.0f);
            }
            return s.h;
        }
    }

    // LEDs
    for (int i = 0; i < 2; ++i)
    {
        const auto& sw = L::switches[i];
        const float dx = x - sw.ledX, dy = y - sw.ledY;
        const float r = std::sqrt (dx * dx + dy * dy);
        const float rl = L::ledRadius, rb2 = rl + 3.0f;
        if (r < rb2)
        {
            s.paint = 0; s.cx = sw.ledX; s.cy = sw.ledY;
            if (r < rl)
            {
                const float u = r / rl;
                s.h = 2.2f + 2.6f * std::sqrt (std::max (0.0f, 1.0f - u * u));
                s.mat = M_LED; s.led = i;
            }
            else
            {
                const float u = (r - rl) / 3.0f;
                s.h = 2.8f * std::sqrt (std::max (0.0f, 1.0f - (2 * u - 1) * (2 * u - 1)));
                s.mat = M_CHROME;
            }
            return s.h;
        }
    }

    return s.h;
}

//==============================================================================
// Mapa de alturas e de horizonte (sombras / oclusão)
//==============================================================================
struct HorizonMap
{
    int w = 0, h = 0, dirs = 24;
    float scale = 2.0f;                     // px por unidade
    std::vector<float> height;
    std::vector<uint8_t> horizon;           // elevação 0..90° em 0..255

    float elevationAt (int px, int py, float azRad) const
    {
        px = jlimit (0, w - 1, px); py = jlimit (0, h - 1, py);
        float f = azRad / (2.0f * kPi) * (float) dirs;
        f = std::fmod (f + (float) dirs * 4.0f, (float) dirs);
        const int a0 = (int) f % dirs, a1 = (a0 + 1) % dirs;
        const float t = f - std::floor (f);
        const size_t base = ((size_t) py * (size_t) w + (size_t) px) * (size_t) dirs;
        const float e = lerpf ((float) horizon[base + (size_t) a0], (float) horizon[base + (size_t) a1], t);
        return e / 255.0f * (kPi * 0.5f);
    }
};

static void parallelFor (int n, const std::function<void (int)>& fn)
{
    const int threads = std::max (1u, std::thread::hardware_concurrency());
    std::atomic<int> next { 0 };
    std::vector<std::thread> pool;
    for (int t = 0; t < threads; ++t)
        pool.emplace_back ([&] { for (int i; (i = next.fetch_add (1)) < n;) fn (i); });
    for (auto& th : pool) th.join();
}

static HorizonMap buildHorizon (const Scene& sc, int w, int h, float scale, float originX, float originY)
{
    HorizonMap hm;
    hm.w = w; hm.h = h; hm.scale = scale;
    hm.height.resize ((size_t) w * (size_t) h);
    hm.horizon.resize ((size_t) w * (size_t) h * (size_t) hm.dirs);

    parallelFor (h, [&] (int y)
    {
        Surf s;
        for (int x = 0; x < w; ++x)
            hm.height[(size_t) y * (size_t) w + (size_t) x] = evalScene (sc, originX + ((float) x + 0.5f) / scale, originY + ((float) y + 0.5f) / scale, s);
    });

    std::vector<float> steps;
    for (float d = 1; d <= 40; d += 1) steps.push_back (d);
    for (float d = 42; d <= 120; d += 2) steps.push_back (d);
    for (float d = 124; d <= 260; d += 4) steps.push_back (d);

    parallelFor (h, [&] (int y)
    {
        for (int x = 0; x < w; ++x)
        {
            const float h0 = hm.height[(size_t) y * (size_t) w + (size_t) x];
            for (int a = 0; a < hm.dirs; ++a)
            {
                const float az = (float) a / (float) hm.dirs * 2.0f * kPi;
                const float ux = std::cos (az), uy = std::sin (az);
                float best = 0.0f;
                for (float d : steps)
                {
                    const int qx = x + (int) std::lround (ux * d), qy = y + (int) std::lround (uy * d);
                    if (qx < 0 || qy < 0 || qx >= w || qy >= h) break;
                    const float dh = hm.height[(size_t) qy * (size_t) w + (size_t) qx] - h0;
                    if (dh > 0.0f) best = std::max (best, dh / (d / scale));
                    if (best > 40.0f) break;
                }
                const float el = std::atan (best);
                hm.horizon[((size_t) y * (size_t) w + (size_t) x) * (size_t) hm.dirs + (size_t) a] = (uint8_t) jlimit (0, 255, (int) std::lround (el / (kPi * 0.5f) * 255.0f));
            }
        }
    });
    return hm;
}

//==============================================================================
// Sombreamento
//==============================================================================
struct MatParams
{
    bool metal = true;
    V3 f0 { 0.91f, 0.92f, 0.92f };
    float ax = 0.2f, ay = 0.2f;     // rugosidade ao longo da tangente / bitangente
    V3 diffuse { 0, 0, 0 };
    V3 emission { 0, 0, 0 };
    float wide = 0.0f;              // fração de um lobo largo (acetinado), que mostra sombras suaves
};

static float smithLambda (V3 w, float ax, float ay)
{
    const float z2 = w.z * w.z;
    if (z2 <= 1e-6f) return 1e6f;
    return (-1.0f + std::sqrt (1.0f + (ax * ax * w.x * w.x + ay * ay * w.y * w.y) / z2)) * 0.5f;
}

static V3 fresnel (V3 f0, float c)
{
    const float m = std::pow (1.0f - clamp01 (c), 5.0f);
    return f0 + (V3 (1, 1, 1) - f0) * m;
}

// Posição na tela do pixel sendo sombreado (por thread; só para o vetor de visão)
static thread_local float tlViewX = 490.0f, tlViewY = 235.0f;

struct Shader
{
    const Scene& scene;
    const HorizonMap* hm;            // pode ser nulo (sem sombras)
    float hmOriginX = 0, hmOriginY = 0;
    V3 keyDir = normalize (V3 (-0.32f, -0.58f, 0.75f));          // luz principal: alto, à esquerda e atrás
    float keyIntensity = 1.5f;
    int samples = 32;

    float visibility (float x, float y, V3 l) const
    {
        if (l.z <= 0.0f) return 0.0f;
        if (hm == nullptr) return 1.0f;
        const int px = (int) ((x - hmOriginX) * hm->scale), py = (int) ((y - hmOriginY) * hm->scale);
        const float el = std::asin (std::min (1.0f, l.z));
        const float az = std::atan2 (l.y, l.x);
        const float hz = hm->elevationAt (px, py, az);
        return smoothstep (hz - 0.03f, hz + 0.03f, el);
    }

    V3 occludedRadiance() const { return V3 (0.06f, 0.06f, 0.065f); }

    V3 incoming (float x, float y, V3 l) const
    {
        const float v = visibility (x, y, l);
        return environment (l) * v + occludedRadiance() * (1.0f - v);
    }

    // Raio da câmera oblíqua: o ponto da tela (sx, sy) enxerga (sx, sy + z*k, z)
    void trace (float sx, float sy, float& wx, float& wy, float& z) const
    {
        const float k = L::obliqueK;
        Surf tmp;
        for (float zz = 30.0f; zz >= -8.5f; zz -= 0.5f)
        {
            if (evalScene (scene, sx, sy + zz * k, tmp) >= zz)
            {
                float lo = zz, hi = zz + 0.5f;
                for (int i = 0; i < 8; ++i)
                {
                    const float mid = (lo + hi) * 0.5f;
                    if (evalScene (scene, sx, sy + mid * k, tmp) >= mid) lo = mid; else hi = mid;
                }
                z = lo; wx = sx; wy = sy + lo * k;
                return;
            }
        }
        z = -8.0f; wx = sx; wy = sy - 8.0f * k;
    }

    V3 shade (float sx, float sy, Rng& rng, float* coverageOut = nullptr) const
    {
        float x, y, zHit;
        trace (sx, sy, x, y, zHit);

        Surf s;
        const float hTop = evalScene (scene, x, y, s);
        if (coverageOut) *coverageOut = (zHit > 0.05f || (s.mat != M_BRUSHED && s.mat != M_FLOOR)) ? 1.0f : 0.0f;

        V3 n;
        const bool wall = (hTop - zHit) > 0.4f && (s.wallN.x != 0.0f || s.wallN.y != 0.0f);
        if (wall)
        {
            n = s.wallN;
            if (s.mat == M_SPUN || s.mat == M_POLISH) s.mat = M_KNURL;
        }
        else
        {
            const float e = 0.25f;
            Surf tmp;
            const float hx1 = evalScene (scene, x + e, y, tmp), hx0 = evalScene (scene, x - e, y, tmp);
            const float hy1 = evalScene (scene, x, y + e, tmp), hy0 = evalScene (scene, x, y - e, tmp);
            V3 g (-(hx1 - hx0) / (2 * e), -(hy1 - hy0) / (2 * e), 1.0f);
            // Bordas abruptas (topo de paredes): limita a inclinação para virar um filete de luz
            const float gl = std::sqrt (g.x * g.x + g.y * g.y);
            if (gl > 2.5f) { g.x *= 2.5f / gl; g.y *= 2.5f / gl; }
            n = normalize (g);
        }
        tlViewX = sx; tlViewY = sy;

        // Parâmetros do material + microdetalhes
        MatParams mp;
        V3 tangent (1, 0, 0);
        const float rx = x - s.cx, ry = y - s.cy;
        const float rr = std::sqrt (rx * rx + ry * ry) + 1e-4f;

        switch (s.mat)
        {
            case M_FLOOR:
                mp.metal = false; mp.diffuse = V3 (0.012f, 0.012f, 0.013f); mp.f0 = V3 (0.03f, 0.03f, 0.03f); mp.ax = mp.ay = 0.6f;
                break;
            case M_BRUSHED:
            {
                // Escovado ao longo de x: riscos finos com variação lenta
                const float streak = vnoise (x * 0.012f, y * 3.1f) * 0.6f + vnoise (x * 0.05f, y * 9.0f) * 0.4f;
                const float streak2 = vnoise (x * 0.004f + 13.0f, y * 0.9f);
                const float fine = vnoise (x * 0.08f + 7.0f, y * 22.0f);
                n = normalize (n + V3 (0.0f, (streak - 0.5f) * 0.09f + (fine - 0.5f) * 0.05f, 0.0f));
                const float alb = 0.76f + 0.10f * streak + 0.05f * streak2 + 0.04f * fine;
                mp.f0 = V3 (alb, alb * 1.005f, alb * 1.012f);
                mp.ax = 0.05f; mp.ay = 0.26f; mp.wide = 0.35f;
                tangent = V3 (1, 0, 0);
                break;
            }
            case M_SPUN:
            {
                // Torneado: ranhuras circulares
                const float ring = vnoise (rr * 5.0f, 0.5f) * 0.6f + vnoise (rr * 17.0f, 3.5f) * 0.4f;
                n = normalize (n + V3 (rx / rr, ry / rr, 0.0f) * ((ring - 0.5f) * 0.035f));
                const float alb = 0.88f + 0.06f * ring;
                mp.f0 = V3 (alb, alb, alb * 1.01f);
                tangent = V3 (-ry / rr, rx / rr, 0.0f);
                mp.ax = 0.04f; mp.ay = 0.32f; mp.wide = 0.2f;
                break;
            }
            case M_POLISH:
                mp.f0 = V3 (0.93f, 0.93f, 0.94f); mp.ax = mp.ay = 0.05f; tangent = V3 (-ry / rr, rx / rr, 0.0f);
                break;
            case M_KNURL:
                mp.f0 = V3 (0.88f, 0.885f, 0.89f) * (1.0f - 0.35f * s.cavity); mp.ax = mp.ay = 0.22f; mp.wide = 0.25f;
                break;
            case M_GROOVE:
                mp.metal = false; mp.diffuse = V3 (0.02f, 0.02f, 0.022f); mp.f0 = V3 (0.04f, 0.04f, 0.04f); mp.ax = mp.ay = 0.35f;
                break;
            case M_CHROME:
                mp.f0 = V3 (0.95f, 0.95f, 0.96f); mp.ax = mp.ay = 0.035f; tangent = V3 (-ry / rr, rx / rr, 0.0f);
                break;
            case M_SATIN:
                mp.f0 = V3 (0.90f, 0.90f, 0.91f); mp.ax = 0.10f; mp.ay = 0.22f; mp.wide = 0.25f; tangent = V3 (-ry / rr, rx / rr, 0.0f);
                break;
            case M_SLOT:
                mp.metal = false; mp.diffuse = V3 (0.015f, 0.015f, 0.016f); mp.f0 = V3 (0.04f, 0.04f, 0.04f); mp.ax = mp.ay = 0.4f;
                break;
            case M_LED:
            {
                const bool on = scene.ledOn[s.led];
                const V3 c = s.led == 0 ? V3 (1.0f, 0.10f, 0.06f) : V3 (0.12f, 1.0f, 0.35f);
                mp.metal = false; mp.f0 = V3 (0.045f, 0.045f, 0.045f); mp.ax = mp.ay = 0.04f;
                const float u = clamp01 (rr / L::ledRadius);
                if (on)
                    mp.emission = c * (2.4f * (1.0f - 0.45f * u * u)) + (c * 0.5f + V3 (0.5f, 0.5f, 0.5f)) * (0.9f * std::exp (-u * u * 10.0f));
                else
                    mp.diffuse = c * 0.07f;
                break;
            }
        }

        // Serigrafia: tinta grafite fosca sobre o metal
        if (s.paint > 0.001f && (s.mat == M_BRUSHED))
        {
            MatParams paint;
            paint.metal = false;
            paint.diffuse = V3 (0.035f, 0.036f, 0.04f);
            paint.f0 = V3 (0.045f, 0.045f, 0.045f);
            paint.ax = paint.ay = 0.32f;
            const V3 a = integrate (x, y, n, tangent, mp, rng);
            const V3 b = integrate (x, y, n, tangent, paint, rng);
            return a * (1.0f - s.paint) + b * s.paint;
        }

        return integrate (x, y, n, tangent, mp, rng);
    }

    V3 integrate (float x, float y, V3 n, V3 tangent, const MatParams& mp, Rng& rng) const
    {
        if (mp.wide > 0.0f)
        {
            MatParams narrow = mp, wideLobe = mp;
            narrow.wide = 0.0f; wideLobe.wide = 0.0f;
            wideLobe.ax = wideLobe.ay = 0.55f;
            wideLobe.emission = V3();
            narrow.emission = V3();
            Shader copy = *this;
            copy.samples = std::max (4, (int) std::lround ((float) samples * 0.6f));
            const V3 a = copy.integrateLobe (x, y, n, tangent, narrow, rng);
            copy.samples = std::max (4, samples - copy.samples);
            const V3 b = copy.integrateLobe (x, y, n, tangent, wideLobe, rng);
            return a * (1.0f - mp.wide) + b * mp.wide + mp.emission;
        }
        return integrateLobe (x, y, n, tangent, mp, rng);
    }

    V3 integrateLobe (float x, float y, V3 n, V3 tangent, const MatParams& mp, Rng& rng) const
    {
        // Base ortonormal (t, b, n)
        V3 t = normalize (tangent - n * dot (tangent, n));
        V3 b = cross (n, t);

        const V3 vWorld = normalize (V3 ((tlViewX - 490.0f) / 2000.0f, L::obliqueK + (tlViewY - 235.0f) / 1400.0f, 1.0f));
        V3 v (dot (vWorld, t), dot (vWorld, b), dot (vWorld, n));
        if (v.z < 0.02f) v.z = 0.02f;
        v = normalize (v);

        V3 sum;
        const int N = samples;
        for (int i = 0; i < N; ++i)
        {
            const float u1 = std::min (0.9999f, ((float) i + rng.next()) / (float) N);
            const float u2 = rng.next();

            // Microfaceta GGX anisotrópica
            const float phi0 = 2.0f * kPi * u2;
            float phi = std::atan2 (mp.ay * std::sin (phi0), mp.ax * std::cos (phi0));
            const float cp = std::cos (phi), sp = std::sin (phi);
            const float a2inv = cp * cp / (mp.ax * mp.ax) + sp * sp / (mp.ay * mp.ay);
            const float tan2 = u1 / ((1.0f - u1) * a2inv);
            const float cosT = 1.0f / std::sqrt (1.0f + tan2), sinT = std::sqrt (std::max (0.0f, 1.0f - cosT * cosT));
            const V3 hT (sinT * cp, sinT * sp, cosT);
            const float vh = dot (v, hT);
            if (vh <= 0.0f) continue;
            const V3 l = hT * (2.0f * vh) - v;
            if (l.z <= 0.0f) continue;

            const float G = 1.0f / (1.0f + smithLambda (v, mp.ax, mp.ay) + smithLambda (l, mp.ax, mp.ay));
            const V3 F = fresnel (mp.f0, vh);
            const float w = G * vh / (v.z * hT.z);

            const V3 lWorld = t * l.x + b * l.y + n * l.z;
            sum += F * incoming (x, y, lWorld) * w;
        }
        V3 col = sum * (1.0f / (float) N);

        // Luz principal direta (projeta as sombras)
        {
            const V3 lw = keyDir;
            const V3 l (dot (lw, t), dot (lw, b), dot (lw, n));
            if (l.z > 0.0f)
            {
                const float vis = visibility (x, y, lw);
                if (vis > 0.0f)
                {
                    const V3 hh = normalize (v + l);
                    const float hx = hh.x / mp.ax, hy = hh.y / mp.ay, hz = hh.z;
                    const float den = hx * hx + hy * hy + hz * hz;
                    const float D = 1.0f / (kPi * mp.ax * mp.ay * den * den);
                    const float G = 1.0f / (1.0f + smithLambda (v, mp.ax, mp.ay) + smithLambda (l, mp.ax, mp.ay));
                    const V3 F = fresnel (mp.f0, dot (v, hh));
                    const float spec = std::min (D * G / (4.0f * v.z * l.z), 30.0f) * l.z;
                    col += F * (spec * keyIntensity * vis);
                    if (! mp.metal)
                        col += mp.diffuse * (l.z * keyIntensity * vis / kPi);
                }
            }
        }

        if (! mp.metal)
        {
            // Difuso (Lambert, amostragem cosseno)
            V3 dsum;
            const int ND = std::max (8, N / 2);
            for (int i = 0; i < ND; ++i)
            {
                const float u1 = ((float) i + rng.next()) / (float) ND, u2 = rng.next();
                const float r = std::sqrt (u1), ph = 2.0f * kPi * u2;
                const V3 l (r * std::cos (ph), r * std::sin (ph), std::sqrt (std::max (0.0f, 1.0f - u1)));
                const V3 lWorld = t * l.x + b * l.y + n * l.z;
                dsum += incoming (x, y, lWorld);
            }
            col += mp.diffuse * dsum * (1.0f / (float) ND);
        }
        return col + mp.emission;
    }
};

//==============================================================================
// Imagem / tonemap
//==============================================================================
static float aces (float x)
{
    return clamp01 ((x * (2.51f * x + 0.03f)) / (x * (2.43f * x + 0.59f) + 0.14f));
}
static uint8 toSrgb8 (float linear)
{
    const float v = linear <= 0.0031308f ? linear * 12.92f : 1.055f * std::pow (linear, 1.0f / 2.4f) - 0.055f;
    return (uint8) jlimit (0, 255, (int) std::lround (v * 255.0f));
}
constexpr float kExposure = 0.88f;

static Colour toColour (V3 c)
{
    return Colour (toSrgb8 (aces (c.x * kExposure)), toSrgb8 (aces (c.y * kExposure)), toSrgb8 (aces (c.z * kExposure)));
}

// Renderiza uma região (em unidades) para uma imagem RGB, com supersampling 2x2
static Image renderRegion (const Shader& sh, float x0, float y0, int pw, int ph, float scale,
                           const std::function<V3 (float, float, V3)>& post = nullptr,
                           std::vector<float>* coverage = nullptr)
{
    if (coverage) coverage->assign ((size_t) pw * (size_t) ph, 0.0f);
    Image img (Image::RGB, pw, ph, false);
    std::vector<V3> buf ((size_t) pw * (size_t) ph);
    std::atomic<int> rowsDone { 0 };

    parallelFor (ph, [&] (int py)
    {
        for (int px = 0; px < pw; ++px)
        {
            Rng rng ((uint32_t) (py * 7919 + px * 104729 + 17));
            V3 acc;
            float cov = 0.0f;
            for (int sy = 0; sy < 2; ++sy)
                for (int sx = 0; sx < 2; ++sx)
                {
                    const float ux = x0 + ((float) px + 0.25f + 0.5f * (float) sx) / scale;
                    const float uy = y0 + ((float) py + 0.25f + 0.5f * (float) sy) / scale;
                    float c1 = 0.0f;
                    acc += sh.shade (ux, uy, rng, &c1);
                    cov += c1;
                }
            if (coverage) (*coverage)[(size_t) py * (size_t) pw + (size_t) px] = cov * 0.25f;
            V3 c = acc * 0.25f;
            if (post) c = post (x0 + ((float) px + 0.5f) / scale, y0 + ((float) py + 0.5f) / scale, c);
            buf[(size_t) py * (size_t) pw + (size_t) px] = c;
        }
        const int d = ++rowsDone;
        if (d % 100 == 0) std::printf ("    %d/%d linhas\n", d, ph), std::fflush (stdout);
    });

    for (int py = 0; py < ph; ++py)
        for (int px = 0; px < pw; ++px)
            img.setPixelAt (px, py, toColour (buf[(size_t) py * (size_t) pw + (size_t) px]));
    return img;
}

static bool save (const Image& img, const File& f, bool jpeg, float quality = 0.92f)
{
    f.deleteFile();
    FileOutputStream os (f);
    if (! os.openedOk()) return false;
    if (jpeg)
    {
        JPEGImageFormat fmt;
        fmt.setQuality (quality);
        return fmt.writeImageToStream (img, os);
    }
    return PNGImageFormat().writeImageToStream (img, os);
}

//==============================================================================
// Serigrafia (textos, escalas e linhas) como máscara
//==============================================================================
static Image buildPaintMask (int maskScale)
{
    Image mask (Image::SingleChannel, L::width * maskScale, L::height * maskScale, true);
    Graphics g (mask);
    g.addTransform (AffineTransform::scale ((float) maskScale));
    g.setColour (Colours::white);

    auto font = [] (const char* style, float size, float kern, const char* family = "Inter")
    {
        return Font (FontOptions().withName (family).withStyle (style).withHeight (size).withKerningFactor (kern));
    };
    auto textWidth = [] (const Font& f, const String& s) { return GlyphArrangement::getStringWidth (f, s); };

    // Títulos dos grupos com linhas
    for (auto& grp : L::groups)
    {
        const auto f = font ("SemiBold", 11.0f, 0.32f);
        const float cx = (grp.x1 + grp.x2) * 0.5f;
        const float tw = textWidth (f, grp.title) + 20.0f;
        g.setFont (f);
        g.drawText (grp.title, Rectangle<float> (tw, 16.0f).withCentre ({ cx, L::groupTitleY }), Justification::centred);
        g.fillRect (Rectangle<float> (grp.x1 + 6.0f, L::groupTitleY - 0.5f, cx - tw * 0.5f - grp.x1 - 6.0f, 1.1f));
        g.fillRect (Rectangle<float> (cx + tw * 0.5f, L::groupTitleY - 0.5f, grp.x2 - 6.0f - cx - tw * 0.5f, 1.1f));
    }

    // Escalas e nomes dos knobs
    for (auto& k : L::knobs)
    {
        const float R = L::knobClasses[k.cls].radius;
        for (int i = 0; i <= 10; ++i)
        {
            const float a = (L::knobMinAngleDeg + (L::knobMaxAngleDeg - L::knobMinAngleDeg) * (float) i / 10.0f) * kPi / 180.0f;
            const float len = (i == 0 || i == 5 || i == 10) ? 7.0f : 4.0f;
            const float ux = std::sin (a), uy = -std::cos (a);
            const float by = L::knobBaseY (k);
            g.drawLine (Line<float> (k.cx + ux * (R + 6.0f), by + uy * (R + 6.0f), k.cx + ux * (R + 6.0f + len), by + uy * (R + 6.0f + len)), 1.4f);
        }
        g.setFont (font ("SemiBold", 12.0f, 0.14f));
        g.drawText (k.label, Rectangle<float> (140.0f, 16.0f).withCentre ({ k.cx, L::knobLabelY }), Justification::centred);
    }

    // Separador
    g.fillRect (Rectangle<float> (L::chassisX + 40.0f, L::separatorY - 0.5f, L::chassisW - 80.0f, 1.1f));

    // Footswitches
    for (auto& sw : L::switches)
    {
        g.setFont (font ("SemiBold", 13.0f, 0.18f));
        g.drawText (sw.label, Rectangle<float> (140.0f, 18.0f).withCentre ({ sw.cx, L::switchLabelY }), Justification::centred);
    }

    // Logotipo
    g.setFont (font ("Bold", 46.0f, 0.42f, "Inter Display"));
    g.drawText ("AURA", Rectangle<float> (420.0f, 56.0f).withCentre ({ L::wordmarkX + 8.0f, L::wordmarkY }), Justification::centred);
    g.setFont (font ("Medium", 11.5f, 0.5f));
    g.drawText ("BASS  OVERDRIVE", Rectangle<float> (420.0f, 18.0f).withCentre ({ L::wordmarkX + 3.0f, L::taglineY }), Justification::centred);

    return mask;
}

//==============================================================================
int main (int argc, char* argv[])
{
    ScopedJuceInitialiser_GUI init;

    if (argc < 2)
    {
        std::printf ("uso: AuraRender <pasta de saída> [amostras] [só: bg|knobs|crops]\n");
        return 1;
    }
    const File out = File::getCurrentWorkingDirectory().getChildFile (argv[1]);
    out.createDirectory();
    const int samples = argc > 2 ? String (argv[2]).getIntValue() : 32;
    const String only = argc > 3 ? String (argv[3]) : String();
    const float S = (float) L::renderScale;

    const Image mask = buildPaintMask (4);
    save (mask.convertedToFormat (Image::RGB), out.getChildFile ("debug_mask.png"), false);

    std::vector<uint8_t> maskData ((size_t) mask.getWidth() * (size_t) mask.getHeight());
    {
        const Image::BitmapData bd (mask, Image::BitmapData::readOnly);
        for (int y = 0; y < bd.height; ++y)
            for (int x = 0; x < bd.width; ++x)
                maskData[(size_t) y * (size_t) bd.width + (size_t) x] = *bd.getPixelPointer (x, y);
    }

    Scene sc;
    sc.paintMask = &maskData;
    sc.maskW = mask.getWidth(); sc.maskH = mask.getHeight(); sc.maskScale = 4;
    const float defaultAngle[7] = { 0.0f, 0.6f, 0.0f, 0.0f, 0.0f, -0.8f, 0.8f };
    for (int i = 0; i < 7; ++i) sc.knobAngle[i] = defaultAngle[i];

    std::printf ("Mapa de horizonte...\n");
    const auto hm = buildHorizon (sc, L::width * L::renderScale, L::height * L::renderScale, S, 0.0f, 0.0f);

    Shader sh { sc, &hm };
    sh.samples = samples;

    // LED aceso: brilho no metal em volta (adicionado em linear)
    auto ledGlow = [&] (int led)
    {
        return [&, led] (float x, float y, V3 c)
        {
            const auto& sw = L::switches[led];
            const float d = std::sqrt ((x - sw.ledX) * (x - sw.ledX) + (y - sw.ledY) * (y - sw.ledY));
            if (d < L::ledRadius) return c;
            const V3 col = led == 0 ? V3 (1.0f, 0.06f, 0.03f) : V3 (0.08f, 1.0f, 0.3f);
            const float g = 0.75f * std::exp (-std::pow ((d - L::ledRadius) / 5.0f, 2.0f)) + 0.25f * std::exp (-std::pow ((d - L::ledRadius) / 14.0f, 2.0f));
            return c + col * g;
        };
    };

    if (only.isEmpty() || only == "bg")
    {
        std::printf ("Fundo %dx%d...\n", L::width * L::renderScale, L::height * L::renderScale);
        const auto bg = renderRegion (sh, 0.0f, 0.0f, L::width * L::renderScale, L::height * L::renderScale, S);
        save (bg, out.getChildFile ("background.jpg"), true, 0.93f);
    }

    if (only.isEmpty() || only == "crops")
    {
        for (int i = 0; i < 2; ++i)
        {
            const auto& sw = L::switches[i];
            std::printf ("Footswitch %d pressionado...\n", i);
            Scene pressed = sc;
            pressed.pressed[i] = true;
            Shader sp { pressed, &hm };
            sp.samples = samples;
            const float h = L::switchCropHalf;
            save (renderRegion (sp, sw.cx - h, sw.cy - h, (int) (2 * h * S), (int) (2 * h * S), S),
                  out.getChildFile ("switch_" + String (i) + "_down.jpg"), true, 0.94f);

            std::printf ("LED %d aceso...\n", i);
            Scene lit = sc;
            lit.ledOn[i] = true;
            Shader sl { lit, &hm };
            sl.samples = samples;
            const float lh = L::ledCropHalf;
            save (renderRegion (sl, sw.ledX - lh, sw.ledY - lh, (int) (2 * lh * S), (int) (2 * lh * S), S, ledGlow (i)),
                  out.getChildFile ("led_" + String (i) + "_on.jpg"), true, 0.95f);
        }
    }

    if (only.isEmpty() || only == "knobs")
    {
        // Um knob de referência de cada tamanho
        const int refKnob[3] = { 0, 1, 2 };
        for (int cls = 0; cls < 3; ++cls)
        {
            const auto& k = L::knobs[refKnob[cls]];
            const auto fr = L::knobFrameRect (k);
            const int fw = (int) std::lround (fr.w * S), fh = (int) std::lround (fr.h * S);
            std::printf ("Knob %d (%dx%d px, %d quadros)...\n", cls, fw, fh, L::knobFrames);

            Image strip (Image::RGB, fw, fh * L::knobFrames, true);
            std::vector<float> coverage;
            for (int f = 0; f < L::knobFrames; ++f)
            {
                Scene ks = sc;
                ks.onlyKnob = refKnob[cls];
                ks.onlyKnobX = k.cx;
                const float deg = L::knobMinAngleDeg + (L::knobMaxAngleDeg - L::knobMinAngleDeg) * (float) f / (float) (L::knobFrames - 1);
                ks.knobAngle[refKnob[cls]] = deg * kPi / 180.0f;
                Shader skn { ks, nullptr };
                skn.samples = samples;
                const auto frame = renderRegion (skn, fr.x, fr.y, fw, fh, S, nullptr, f == 0 ? &coverage : nullptr);
                Graphics g (strip);
                g.drawImageAt (frame, 0, f * fh);
            }
            save (strip, out.getChildFile ("knob_" + String (cls) + ".jpg"), true, 0.93f);

            // Máscara a partir da cobertura real do knob (silhueta igual em todos os quadros)
            Image m (Image::ARGB, fw, fh, true);
            for (int y = 0; y < fh; ++y)
                for (int x = 0; x < fw; ++x)
                    m.setPixelAt (x, y, Colours::white.withAlpha (coverage[(size_t) y * (size_t) fw + (size_t) x]));
            save (m, out.getChildFile ("knob_" + String (cls) + "_mask.png"), false);
        }
    }

    std::printf ("Pronto.\n");
    return 0;
}
