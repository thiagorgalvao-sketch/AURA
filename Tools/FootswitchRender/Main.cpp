// FootswitchRender — renderiza o footswitch do DRIVE (porca sextavada, arruela e botão
// de cromo) com iluminação física, para ser aplicado sobre a arte do painel.
//
// Saídas (2x, centro do footswitch no centro da imagem):
//   footswitch_up.png     RGBA, solto
//   footswitch_down.png   RGBA, pressionado
//   footswitch_shadow.png sombra/oclusão na placa (multiplicador em escala de cinza)
//
// Uso: FootswitchRender <pasta de saída> [amostras]

#include <juce_graphics/juce_graphics.h>
#include <atomic>
#include <cmath>
#include <functional>
#include <thread>
#include <vector>

using namespace juce;

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

struct Rng
{
    uint32_t s;
    explicit Rng (uint32_t seed) : s (seed * 747796405u + 2891336453u) {}
    float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s & 0xffffff) / 16777216.0f; }
};

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
    return lerpf (lerpf (hash2 (xi, yi), hash2 (xi + 1, yi), ux), lerpf (hash2 (xi, yi + 1), hash2 (xi + 1, yi + 1), ux), uy);
}

//==============================================================================
// Estúdio escuro (como a arte do painel): softbox no alto, strips laterais
static V3 environment (V3 d)
{
    if (d.z <= 0.0f) return V3 (0.012f, 0.012f, 0.014f);
    const float el = std::asin (std::min (1.0f, d.z)) * 180.0f / kPi;
    const float az = std::atan2 (-d.y, d.x) * 180.0f / kPi;
    auto azDiff = [] (float a, float b) { return std::fmod (a - b + 540.0f, 360.0f) - 180.0f; };
    float v = 0.05f + 0.05f * d.z;
    V3 col (v, v * 1.01f, v * 1.05f);
    auto box = [&] (float cAz, float cEl, float hAz, float hEl, float soft, float intensity)
    {
        const float dx = azDiff (az, cAz) * std::cos (el * kPi / 180.0f);
        const float dy = el - cEl;
        col += V3 (1, 1, 1) * (intensity * (1.0f - smoothstep (hAz - soft, hAz + soft, std::abs (dx)))
                                         * (1.0f - smoothstep (hEl - soft, hEl + soft, std::abs (dy))));
    };
    {
        const float mx = 1.0f - smoothstep (0.25f, 0.55f, std::abs (d.x));
        const float my = 1.0f - smoothstep (0.05f, 0.28f, std::abs (d.y + 0.32f));
        col += V3 (1.0f, 0.99f, 0.97f) * (1.1f * mx * my);
    }
    box (-90.0f, 30.0f, 50.0f, 10.0f, 6.0f, 0.35f);
    box (0.0f, 30.0f, 5.0f, 22.0f, 3.0f, 1.4f);
    box (180.0f, 26.0f, 4.0f, 18.0f, 3.0f, 0.8f);
    box (90.0f, 20.0f, 40.0f, 8.0f, 5.0f, 0.35f);
    return col;
}

//==============================================================================
// Geometria (unidades = px lógicos, centro em 0,0)
enum Mat { M_PLATE, M_SATIN, M_CHROME, M_GAP };

struct Surf { float h = 0; Mat mat = M_PLATE; V3 wallN; };

constexpr float kR = 22.0f;       // porca sextavada até os cantos
constexpr float kObliqueK = 0.12f;

static float hexSdf (float x, float y, float r)
{
    const float k1 = 0.8660254f, k2 = 0.5f;
    x = std::abs (x); y = std::abs (y);
    const float ap = r * k1;
    return std::max (y - ap, x * k1 + y * k2 - ap);
}

struct Scene { bool present = true; bool pressed = false; };

static float evalScene (const Scene& sc, float x, float y, Surf& s)
{
    s = Surf();
    if (! sc.present) return 0.0f;
    const float r = std::sqrt (x * x + y * y);
    const float hd = hexSdf (x, y, kR);
    if (hd >= 0.0f) return 0.0f;

    s.wallN = V3 (x / (r + 1e-4f), y / (r + 1e-4f), 0.0f);
    const float rW = 0.80f * kR, rD = 0.56f * kR;
    if (r < rD)
    {
        const float top = sc.pressed ? 7.0f : 9.0f;
        const float u = r / rD;
        s.h = top + 3.4f * std::sqrt (std::max (0.0f, 1.0f - u * u));
        s.mat = M_CHROME;
    }
    else if (r < rD + 0.8f) { s.h = 5.5f; s.mat = M_GAP; }
    else if (r < rW)
    {
        const float u = (r - rD) / (rW - rD);
        s.h = 6.4f - 1.8f * u * u;
        s.mat = M_CHROME;
    }
    else
    {
        const float bev = 1.3f;
        s.h = 4.2f - (hd > -bev ? (bev + hd) * (bev + hd) / bev : 0.0f);
        s.mat = M_SATIN;
        const float a = std::atan2 (y, x);
        const float sector = std::round ((a - kPi / 6.0f) / (kPi / 3.0f));
        const float na = kPi / 6.0f + sector * (kPi / 3.0f);
        s.wallN = V3 (std::cos (na), std::sin (na), 0.0f);
    }
    return s.h;
}

//==============================================================================
struct Horizon
{
    int n = 0, dirs = 24; float scale = 2.0f, half = 46.0f;
    std::vector<uint8_t> data;
    float at (float x, float y, float az) const
    {
        const int px = jlimit (0, n - 1, (int) ((x + half) * scale)), py = jlimit (0, n - 1, (int) ((y + half) * scale));
        float f = std::fmod (az / (2 * kPi) * (float) dirs + (float) dirs * 4.0f, (float) dirs);
        const int a0 = (int) f % dirs, a1 = (a0 + 1) % dirs;
        const size_t b = ((size_t) py * (size_t) n + (size_t) px) * (size_t) dirs;
        return lerpf (data[b + (size_t) a0], data[b + (size_t) a1], f - std::floor (f)) / 255.0f * kPi * 0.5f;
    }
};

static void parallelFor (int n, const std::function<void (int)>& fn)
{
    std::atomic<int> next { 0 };
    std::vector<std::thread> pool;
    for (unsigned t = 0; t < std::max (1u, std::thread::hardware_concurrency()); ++t)
        pool.emplace_back ([&] { for (int i; (i = next.fetch_add (1)) < n;) fn (i); });
    for (auto& th : pool) th.join();
}

static Horizon buildHorizon (const Scene& sc, float half, float scale)
{
    Horizon hz; hz.scale = scale; hz.half = half; hz.n = (int) (2 * half * scale);
    std::vector<float> h ((size_t) hz.n * (size_t) hz.n);
    hz.data.resize ((size_t) hz.n * (size_t) hz.n * (size_t) hz.dirs);
    Surf s;
    for (int y = 0; y < hz.n; ++y)
        for (int x = 0; x < hz.n; ++x)
            h[(size_t) y * (size_t) hz.n + (size_t) x] = evalScene (sc, ((float) x + 0.5f) / scale - half, ((float) y + 0.5f) / scale - half, s);
    parallelFor (hz.n, [&] (int y)
    {
        for (int x = 0; x < hz.n; ++x)
        {
            const float h0 = h[(size_t) y * (size_t) hz.n + (size_t) x];
            for (int a = 0; a < hz.dirs; ++a)
            {
                const float az = (float) a / (float) hz.dirs * 2 * kPi;
                float best = 0;
                for (float d = 1; d < 90; d += 1)
                {
                    const int qx = x + (int) std::lround (std::cos (az) * d), qy = y + (int) std::lround (std::sin (az) * d);
                    if (qx < 0 || qy < 0 || qx >= hz.n || qy >= hz.n) break;
                    const float dh = h[(size_t) qy * (size_t) hz.n + (size_t) qx] - h0;
                    if (dh > 0) best = std::max (best, dh / (d / scale));
                }
                hz.data[((size_t) y * (size_t) hz.n + (size_t) x) * (size_t) hz.dirs + (size_t) a] = (uint8_t) jlimit (0, 255, (int) std::lround (std::atan (best) / (kPi * 0.5f) * 255.0f));
            }
        }
    });
    return hz;
}

//==============================================================================
struct MatP { V3 f0; float ax, ay; bool metal = true; V3 diffuse; float wide = 0; };

static float lambda (V3 w, float ax, float ay)
{
    const float z2 = w.z * w.z;
    if (z2 <= 1e-6f) return 1e6f;
    return (-1.0f + std::sqrt (1.0f + (ax * ax * w.x * w.x + ay * ay * w.y * w.y) / z2)) * 0.5f;
}
static V3 fresnel (V3 f0, float c) { const float m = std::pow (1.0f - clamp01 (c), 5.0f); return f0 + (V3 (1, 1, 1) - f0) * m; }

struct Shader
{
    const Scene& sc;
    const Horizon& hz;
    int samples = 64;
    V3 key = normalize (V3 (-0.30f, -0.60f, 0.74f));
    float keyI = 1.4f;

    float vis (float x, float y, V3 l) const
    {
        if (l.z <= 0) return 0;
        const float el = std::asin (std::min (1.0f, l.z));
        const float h = hz.at (x, y, std::atan2 (l.y, l.x));
        return smoothstep (h - 0.03f, h + 0.03f, el);
    }
    V3 incoming (float x, float y, V3 l) const { const float v = vis (x, y, l); return environment (l) * v + V3 (0.03f, 0.03f, 0.035f) * (1 - v); }

    void trace (float sx, float sy, float& x, float& y, float& z) const
    {
        Surf t;
        for (float zz = 16.0f; zz >= -0.5f; zz -= 0.25f)
            if (evalScene (sc, sx, sy + zz * kObliqueK, t) >= zz)
            {
                float lo = zz, hi = zz + 0.25f;
                for (int i = 0; i < 8; ++i) { const float m = (lo + hi) * 0.5f; if (evalScene (sc, sx, sy + m * kObliqueK, t) >= m) lo = m; else hi = m; }
                x = sx; y = sy + lo * kObliqueK; z = lo; return;
            }
        x = sx; y = sy; z = 0;
    }

    V3 lobe (float x, float y, V3 n, V3 tg, const MatP& m, Rng& rng, int N) const
    {
        const V3 t = normalize (tg - n * dot (tg, n)), b = cross (n, t);
        const V3 vw = normalize (V3 (0.0f, kObliqueK, 1.0f));
        V3 v (dot (vw, t), dot (vw, b), dot (vw, n));
        v.z = std::max (v.z, 0.02f); v = normalize (v);
        V3 sum;
        for (int i = 0; i < N; ++i)
        {
            const float u1 = std::min (0.9999f, ((float) i + rng.next()) / (float) N), u2 = rng.next();
            const float ph = std::atan2 (m.ay * std::sin (2 * kPi * u2), m.ax * std::cos (2 * kPi * u2));
            const float cp = std::cos (ph), sp = std::sin (ph);
            const float tan2 = u1 / ((1 - u1) * (cp * cp / (m.ax * m.ax) + sp * sp / (m.ay * m.ay)));
            const float ct = 1 / std::sqrt (1 + tan2), st = std::sqrt (std::max (0.0f, 1 - ct * ct));
            const V3 h (st * cp, st * sp, ct);
            const float vh = dot (v, h);
            if (vh <= 0) continue;
            const V3 l = h * (2 * vh) - v;
            if (l.z <= 0) continue;
            const float G = 1 / (1 + lambda (v, m.ax, m.ay) + lambda (l, m.ax, m.ay));
            sum += fresnel (m.f0, vh) * incoming (x, y, t * l.x + b * l.y + n * l.z) * (G * vh / (v.z * h.z));
        }
        V3 col = sum * (1.0f / (float) N);
        const V3 l (dot (key, t), dot (key, b), dot (key, n));
        if (l.z > 0)
        {
            const float kv = vis (x, y, key);
            const V3 hh = normalize (v + l);
            const float hx = hh.x / m.ax, hy = hh.y / m.ay, den = hx * hx + hy * hy + hh.z * hh.z;
            const float D = 1 / (kPi * m.ax * m.ay * den * den);
            const float G = 1 / (1 + lambda (v, m.ax, m.ay) + lambda (l, m.ax, m.ay));
            col += fresnel (m.f0, dot (v, hh)) * (std::min (D * G / (4 * v.z * l.z), 30.0f) * l.z * keyI * kv);
            if (! m.metal) col += m.diffuse * (l.z * keyI * kv / kPi);
        }
        if (! m.metal)
        {
            V3 d;
            for (int i = 0; i < 16; ++i)
            {
                const float u1 = ((float) i + rng.next()) / 16.0f, ph = 2 * kPi * rng.next(), r = std::sqrt (u1);
                const V3 ll (r * std::cos (ph), r * std::sin (ph), std::sqrt (1 - u1));
                d += incoming (x, y, t * ll.x + b * ll.y + n * ll.z);
            }
            col += m.diffuse * d * (1.0f / 16.0f);
        }
        return col;
    }

    // Irradiância difusa na placa (para a razão de sombra)
    float plateIrradiance (float x, float y, Rng& rng) const
    {
        float e = 0;
        const int N = 96;
        for (int i = 0; i < N; ++i)
        {
            const float u1 = ((float) i + rng.next()) / (float) N, ph = 2 * kPi * rng.next(), r = std::sqrt (u1);
            const V3 l (r * std::cos (ph), r * std::sin (ph), std::sqrt (1 - u1));
            const V3 c = incoming (x, y, l);
            e += (c.x + c.y + c.z) / 3.0f;
        }
        e /= (float) N;
        return e + key.z * keyI * vis (x, y, key) / kPi * 1.0f;
    }

    V3 shade (float sx, float sy, Rng& rng, float& cov) const
    {
        float x, y, z;
        trace (sx, sy, x, y, z);
        Surf s;
        const float hTop = evalScene (sc, x, y, s);
        cov = (s.mat != M_PLATE) ? 1.0f : 0.0f;
        if (s.mat == M_PLATE) return V3();

        V3 n;
        if (hTop - z > 0.35f) n = s.wallN;
        else
        {
            const float e = 0.2f; Surf t;
            V3 g (-(evalScene (sc, x + e, y, t) - evalScene (sc, x - e, y, t)) / (2 * e),
                  -(evalScene (sc, x, y + e, t) - evalScene (sc, x, y - e, t)) / (2 * e), 1.0f);
            const float gl = std::sqrt (g.x * g.x + g.y * g.y);
            if (gl > 2.5f) { g.x *= 2.5f / gl; g.y *= 2.5f / gl; }
            n = normalize (g);
        }
        const float r = std::sqrt (x * x + y * y) + 1e-4f;
        const V3 circ (-y / r, x / r, 0);
        MatP m;
        switch (s.mat)
        {
            case M_CHROME:
            {
                const float ring = vnoise (r * 6.0f, 0.5f);
                m.f0 = V3 (0.93f, 0.93f, 0.945f) * (0.97f + 0.03f * ring); m.ax = 0.035f; m.ay = 0.06f;
                break;
            }
            case M_SATIN: m.f0 = V3 (0.82f, 0.82f, 0.84f); m.ax = 0.10f; m.ay = 0.22f; m.wide = 0.25f; break;
            case M_GAP:   m.metal = false; m.f0 = V3 (0.04f, 0.04f, 0.04f); m.ax = m.ay = 0.4f; m.diffuse = V3 (0.01f, 0.01f, 0.012f); break;
            default: break;
        }
        if (m.wide > 0)
        {
            MatP w = m; w.ax = w.ay = 0.55f;
            const int N1 = (int) (samples * 0.6f);
            return lobe (x, y, n, circ, m, rng, N1) * (1 - m.wide) + lobe (x, y, n, circ, w, rng, samples - N1) * m.wide;
        }
        return lobe (x, y, n, circ, m, rng, samples);
    }
};

static float aces (float x) { return clamp01 ((x * (2.51f * x + 0.03f)) / (x * (2.43f * x + 0.59f) + 0.14f)); }
static uint8 srgb (float l) { const float v = l <= 0.0031308f ? l * 12.92f : 1.055f * std::pow (l, 1 / 2.4f) - 0.055f; return (uint8) jlimit (0, 255, (int) std::lround (v * 255)); }

static void save (const Image& img, const File& f) { f.deleteFile(); FileOutputStream os (f); PNGImageFormat().writeImageToStream (img, os); }

int main (int argc, char* argv[])
{
    ScopedJuceInitialiser_GUI init;
    if (argc < 2) { std::printf ("uso: FootswitchRender <pasta> [amostras]\n"); return 1; }
    const File out = File::getCurrentWorkingDirectory().getChildFile (argv[1]);
    out.createDirectory();
    const int samples = argc > 2 ? String (argv[2]).getIntValue() : 64;
    const float half = 46.0f, scale = 2.0f;
    const int n = (int) (2 * half * scale);

    for (int pass = 0; pass < 2; ++pass)
    {
        Scene sc; sc.pressed = pass == 1;
        const auto hz = buildHorizon (sc, half, scale);
        Shader sh { sc, hz }; sh.samples = samples;
        Image img (Image::ARGB, n, n, true);
        std::vector<V3> col ((size_t) n * (size_t) n);
        std::vector<float> cov ((size_t) n * (size_t) n);
        parallelFor (n, [&] (int py)
        {
            for (int px = 0; px < n; ++px)
            {
                Rng rng ((uint32_t) (py * 7919 + px * 104729 + 3 + pass));
                V3 acc; float c = 0;
                for (int s = 0; s < 4; ++s)
                {
                    float c1;
                    const float ux = ((float) px + 0.25f + 0.5f * (float) (s & 1)) / scale - half;
                    const float uy = ((float) py + 0.25f + 0.5f * (float) (s >> 1)) / scale - half;
                    acc += sh.shade (ux, uy, rng, c1);
                    c += c1;
                }
                cov[(size_t) py * (size_t) n + (size_t) px] = c * 0.25f;
                col[(size_t) py * (size_t) n + (size_t) px] = c > 0 ? acc * (1.0f / c) : V3();
            }
        });
        for (int y = 0; y < n; ++y)
            for (int x = 0; x < n; ++x)
            {
                const auto c = col[(size_t) y * (size_t) n + (size_t) x];
                img.setPixelAt (x, y, Colour (srgb (aces (c.x)), srgb (aces (c.y)), srgb (aces (c.z))).withAlpha (cov[(size_t) y * (size_t) n + (size_t) x]));
            }
        save (img, out.getChildFile (pass == 0 ? "footswitch_up.png" : "footswitch_down.png"));

        if (pass == 0)
        {
            // Sombra: irradiância da placa com o footswitch / sem ele
            Scene empty; empty.present = false;
            const auto hzE = buildHorizon (empty, half, scale);
            Shader she { empty, hzE };
            Image shadow (Image::ARGB, n, n, true);
            parallelFor (n, [&] (int py)
            {
                for (int px = 0; px < n; ++px)
                {
                    const float ux = ((float) px + 0.5f) / scale - half, uy = ((float) py + 0.5f) / scale - half;
                    Rng r1 ((uint32_t) (py * 31 + px * 17 + 1)), r2 ((uint32_t) (py * 31 + px * 17 + 1));
                    const float ratio = clamp01 (sh.plateIrradiance (ux, uy, r1) / std::max (1e-4f, she.plateIrradiance (ux, uy, r2)));
                    const uint8 g = (uint8) jlimit (0, 255, (int) std::lround (ratio * 255.0f));
                    shadow.setPixelAt (px, py, Colour (g, g, g));
                }
            });
            save (shadow, out.getChildFile ("footswitch_shadow.png"));
        }
        std::printf ("passo %d ok\n", pass);
    }
    return 0;
}
