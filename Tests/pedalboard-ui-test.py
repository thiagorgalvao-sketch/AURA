#!/usr/bin/env python3
# WORSHIP BASS ESSENTIALS — teste de interface e de áudio num Chromium de verdade.
# Copyright (c) 2026 Thiago Galvão. Todos os direitos reservados.
# Licença: ver LICENSE  |  https://github.com/thiagorgalvao-sketch/AURA
#
# Uso: python3 Tests/pedalboard-ui-test.py [pasta do site] [pasta de saída]
# Precisa do Playwright com Chromium instalado.

import http.server
import math
import os
import struct
import sys
import threading
import wave

from playwright.sync_api import sync_playwright

SITE = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "site")
OUT = os.path.abspath(sys.argv[2] if len(sys.argv) > 2 else "pedalboard-test")
os.makedirs(OUT, exist_ok=True)

falhas = []


def check(ok, oque):
    print(f"  [{' OK ' if ok else 'FALHA'}] {oque}")
    if not ok:
        falhas.append(oque)


def baixo_wav(path, hz=41.203, segundos=14, rate=48000):
    """Baixo em E1 com harmônicos e ataque a cada 3 s."""
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        frames = bytearray()
        for i in range(int(rate * segundos)):
            t = i / rate
            env = 0.35 + 0.65 * math.exp(-((t % 3.0) * 1.2))
            ph = 2 * math.pi * hz * t
            s = 0.6 * math.sin(ph) + 0.22 * math.sin(2 * ph + 0.7) + 0.1 * math.sin(3 * ph + 1.9)
            frames += struct.pack("<h", int(max(-1, min(1, s * env * 0.6)) * 32000))
        w.writeframes(bytes(frames))


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *a, **kw):
        super().__init__(*a, directory=SITE, **kw)

    def log_message(self, *a):
        pass


# Mede o pico da saída do pedalboard por alguns instantes, gravando o destino.
MEDE = """
async (ms) => {
  const ctx = window.__pb.audio, rig = window.__pb.rig;
  if (!ctx || !rig) return -1;
  const an = ctx.createAnalyser();
  an.fftSize = 2048;
  rig.saida.connect(an);
  const buf = new Float32Array(an.fftSize);
  let pico = 0;
  const ate = performance.now() + ms;
  while (performance.now() < ate) {
    an.getFloatTimeDomainData(buf);
    for (let i = 0; i < buf.length; i++) pico = Math.max(pico, Math.abs(buf[i]));
    await new Promise((r) => setTimeout(r, 30));
  }
  rig.saida.disconnect(an);
  return pico;
}
"""


def main():
    wav = os.path.join(OUT, "baixo-e1.wav")
    baixo_wav(wav)

    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 8731), Handler)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    url = "http://127.0.0.1:8731/pedalboard/index.html"

    with sync_playwright() as pw:
        browser = pw.chromium.launch(args=[
            "--no-sandbox",
            "--use-fake-ui-for-media-stream",
            "--use-fake-device-for-media-stream",
            f"--use-file-for-fake-audio-capture={wav}%noloop",
            "--autoplay-policy=no-user-gesture-required",
        ])
        page = browser.new_page(viewport={"width": 1280, "height": 860}, device_scale_factor=2)

        erros = []
        page.on("console", lambda m: erros.append(m.text) if m.type == "error" else None)
        page.on("pageerror", lambda e: erros.append(str(e)))

        print("Carregamento da página")
        page.goto(url, wait_until="networkidle")
        page.wait_for_timeout(500)
        check(page.is_visible("#inicio"), "tela inicial aparece")
        check(len(page.query_selector_all(".preset")) == 8, "os oito presets aparecem")
        check(len(page.query_selector_all(".pedal")) == 10, "os dez pedais aparecem")
        check(not erros, f"sem erros no console: {erros[:2]}")
        page.screenshot(path=f"{OUT}/1-inicio.png")

        print("Início do áudio")
        page.click("#botaoLigar")
        page.wait_for_timeout(2500)
        check(page.evaluate("() => document.getElementById('inicio').hidden"), "áudio iniciou")
        estado_ctx = page.evaluate("() => window.__pb.audio && window.__pb.audio.state")
        check(estado_ctx == "running", f"contexto de áudio rodando ({estado_ctx})")
        page.screenshot(path=f"{OUT}/2-pedalboard.png")

        print("A cadeia produz som")
        pico = page.evaluate(MEDE, 1200)
        print(f"   pico na saída: {pico:.3f}")
        check(pico > 0.01, "sai áudio do pedalboard")

        print("Cada preset liga os pedais certos")
        esperado = {
            "colorindo": ["consoleEq", "comp", "eq", "amp"],
            "caixa-suja": ["consoleEq", "comp", "eq", "amp", "drive"],
            "rato": ["consoleEq", "comp", "eq", "amp", "fuzz", "eq2"],
            "octavio": ["consoleEq", "comp", "eq", "amp", "sub", "eq2"],
            "macro": ["consoleEq", "comp", "eq", "amp", "gate", "sub", "fuzz"],
            "chora": ["consoleEq", "comp", "eq", "amp", "drive", "chorus"],
            "tudo": ["consoleEq", "comp", "eq", "amp", "drive", "fuzz", "eq2", "gate", "sub", "chorus"],
            "moog": ["consoleEq", "comp"],
        }
        for pid, cadeia in esperado.items():
            page.evaluate("(id) => window.__pb.aplicaPreset(id)", pid)
            page.wait_for_timeout(120)
            ligados = page.evaluate(
                "() => Object.entries(window.__pb.estado.pedais).filter(([,v]) => v.on).map(([k]) => k)")
            check(sorted(ligados) == sorted(cadeia), f"preset {pid}: {', '.join(cadeia)}")

        print("Os presets soam diferentes entre si")
        picos = {}
        for pid in ["colorindo", "caixa-suja", "tudo"]:
            page.evaluate("(id) => window.__pb.aplicaPreset(id)", pid)
            page.wait_for_timeout(400)
            picos[pid] = page.evaluate(MEDE, 900)
            print(f"   {pid}: pico {picos[pid]:.3f}")
        check(all(v > 0.005 for v in picos.values()), "todos os presets produzem som")
        check(all(v <= 1.0 for v in picos.values()), "nenhum preset passa de 0 dBFS na saída")

        print("A oitava abaixo aparece no espectro")
        page.evaluate("(id) => window.__pb.aplicaPreset(id)", "octavio")
        page.wait_for_timeout(700)
        sub = page.evaluate("""
        async () => {
          const ctx = window.__pb.audio, rig = window.__pb.rig;
          const an = ctx.createAnalyser();
          an.fftSize = 32768; an.smoothingTimeConstant = 0.6;
          rig.saida.connect(an);
          await new Promise((r) => setTimeout(r, 1200));
          const d = new Float32Array(an.frequencyBinCount);
          an.getFloatFrequencyData(d);
          rig.saida.disconnect(an);
          const hz2bin = (hz) => Math.round(hz / (ctx.sampleRate / an.fftSize));
          const nivel = (hz) => {
            const b = hz2bin(hz);
            let m = -200;
            for (let i = b - 2; i <= b + 2; i++) m = Math.max(m, d[i] || -200);
            return m;
          };
          return { sub: nivel(20.6), fund: nivel(41.2) };
        }
        """)
        print(f"   20,6 Hz: {sub['sub']:.1f} dB   41,2 Hz: {sub['fund']:.1f} dB")
        check(sub["sub"] > sub["fund"] - 25 and sub["sub"] > -90, "a oitava abaixo (20,6 Hz) está presente")

        print("Footswitch e knobs respondem")
        page.evaluate("(id) => window.__pb.aplicaPreset(id)", "caixa-suja")
        page.wait_for_timeout(200)
        page.click('[data-pedal="drive"] .foot')
        page.wait_for_timeout(150)
        check(page.evaluate("() => window.__pb.estado.pedais.drive.on") is False, "footswitch desliga o pedal")
        page.click('[data-pedal="drive"] .foot')
        page.wait_for_timeout(150)
        check(page.evaluate("() => window.__pb.estado.pedais.drive.on") is True, "footswitch liga de novo")

        knob = page.locator('[data-pedal="amp"] [data-knob="bass"]')
        caixa = knob.bounding_box()
        antes = page.evaluate("() => window.__pb.estado.pedais.amp.params.bass")
        page.mouse.move(caixa["x"] + caixa["width"] / 2, caixa["y"] + caixa["height"] / 2)
        page.mouse.down()
        page.mouse.move(caixa["x"] + caixa["width"] / 2, caixa["y"] + caixa["height"] / 2 - 40, steps=8)
        page.mouse.up()
        depois = page.evaluate("() => window.__pb.estado.pedais.amp.params.bass")
        print(f"   BASS: {antes:.1f} -> {depois:.1f}")
        check(depois > antes, "arrastar o knob aumenta o valor")

        print("Menu e estado salvo")
        page.click("#botaoMenu")
        page.wait_for_timeout(200)
        check(not page.evaluate("() => document.getElementById('menu').hidden"), "menu abre")
        page.wait_for_timeout(900)
        sels = page.eval_on_selector_all(
            "#menu select",
            "els => els.map(e => [...e.options].map(o => o.textContent))")
        print(f"   seletores de dispositivo: {len(sels)}")
        for lista in sels:
            print("     " + ", ".join(lista))
        check(len(sels) == 2, "menu traz entrada e saída de áudio")
        check(any(len(l) > 1 for l in sels), "os dispositivos do sistema aparecem na lista")
        page.keyboard.press("Escape")
        page.evaluate("(id) => window.__pb.aplicaPreset(id)", "chora")
        page.wait_for_timeout(300)
        page.screenshot(path=f"{OUT}/3-chora.png")
        page.reload(wait_until="networkidle")
        page.wait_for_timeout(600)
        check(page.evaluate("() => window.__pb.estado.preset") == "chora", "preset volta após recarregar")

        print("Telas pequenas")
        for nome, w, h in [("celular", 390, 844), ("tablet", 1024, 768)]:
            page.set_viewport_size({"width": w, "height": h})
            page.wait_for_timeout(300)
            cabe = page.evaluate("""() => {
                const b = document.getElementById('board').getBoundingClientRect();
                return b.left >= -1 && b.right <= window.innerWidth + 1;
            }""")
            check(cabe, f"pedais cabem na tela ({nome}: {w}x{h})")
            page.screenshot(path=f"{OUT}/4-{nome}.png", full_page=True)

        check(not erros, f"sem erros no console ao final: {erros[:3]}")
        browser.close()

    srv.shutdown()
    print("\n" + ("Todos os testes passaram." if not falhas else f"{len(falhas)} teste(s) falharam."))
    return 1 if falhas else 0


if __name__ == "__main__":
    sys.exit(main())
