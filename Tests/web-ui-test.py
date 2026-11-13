#!/usr/bin/env python3
# AURA Bass — teste de interface da versão web
# Copyright (c) 2026 Thiago Galvão. Todos os direitos reservados.
# Obra protegida pelas Leis 9.609/1998 e 9.610/1998. Uso pessoal permitido;
# cópia, redistribuição ou obra derivada apenas com autorização por escrito.
# Licença: ver LICENSE  |  https://github.com/thiagorgalvao-sketch/AURA
"""
Teste da versão web num navegador real (Chromium), com uma entrada de áudio falsa
tocando um Mi grave de baixo (E1, 41,2 Hz).

Verifica: a página carrega sem erros, o áudio inicia, o afinador reconhece a nota,
os controles respondem ao toque e o app funciona sem internet (service worker).
Salva imagens da tela em <saída>/.

Uso: python3 -I Tests/web-ui-test.py <pasta do site> <pasta de saída>
"""
import http.server
import math
import os
import struct
import subprocess
import sys
import threading
import wave

from playwright.sync_api import sync_playwright

SITE = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "site")
OUT = os.path.abspath(sys.argv[2] if len(sys.argv) > 2 else "web-test")
os.makedirs(OUT, exist_ok=True)

failures = []


def check(ok, what):
    print(f"  [{' OK ' if ok else 'FALHA'}] {what}")
    if not ok:
        failures.append(what)


def make_bass_wav(path, hz=41.203, seconds=12, rate=48000):
    """Nota de baixo com harmônicos e um ataque, parecida com uma corda tocada."""
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        frames = bytearray()
        for i in range(int(rate * seconds)):
            t = i / rate
            env = 0.4 + 0.6 * math.exp(-((t % 3.0) * 1.2))
            ph = 2 * math.pi * hz * t
            s = 0.55 * math.sin(ph) + 0.25 * math.sin(2 * ph + 0.7) + 0.12 * math.sin(3 * ph + 1.9)
            frames += struct.pack("<h", int(max(-1, min(1, s * env * 0.6)) * 32000))
        w.writeframes(bytes(frames))


class Handler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *a, **kw):
        super().__init__(*a, directory=SITE, **kw)

    def log_message(self, *a):
        pass


def main():
    wav = os.path.join(OUT, "bass-e1.wav")
    make_bass_wav(wav)

    server = http.server.ThreadingHTTPServer(("127.0.0.1", 8723), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    url = "http://127.0.0.1:8723/index.html"

    with sync_playwright() as pw:
        browser = pw.chromium.launch(args=[
            "--no-sandbox",
            "--use-fake-ui-for-media-stream",
            "--use-fake-device-for-media-stream",
            f"--use-file-for-fake-audio-capture={wav}%noloop",
            "--autoplay-policy=no-user-gesture-required",
        ])
        page = browser.new_page(viewport={"width": 1280, "height": 720}, device_scale_factor=2)

        errors = []
        page.on("console", lambda m: errors.append(m.text) if m.type == "error" else None)
        page.on("pageerror", lambda e: errors.append(str(e)))

        print("Carregamento da página")
        page.goto(url, wait_until="networkidle")
        page.wait_for_timeout(600)
        check(page.is_visible("#start"), "tela inicial aparece")
        check(not errors, f"sem erros no console: {errors[:2]}")
        page.screenshot(path=f"{OUT}/1-inicio.png")

        print("Escolha da entrada antes de ligar")
        page.click("#startButton")
        page.wait_for_timeout(1500)
        check(page.is_visible("#escolhaEntrada"), "a lista de entradas aparece")
        check(page.evaluate("() => document.getElementById('start').classList.contains('hidden')") is False,
              "o áudio NÃO começa sozinho no microfone do aparelho")
        opcoes = page.eval_on_selector_all("#entradaSelect option", "els => els.map(e => e.textContent)")
        print("   entradas:", ", ".join(opcoes))
        check(len(opcoes) > 1, "as entradas do sistema estão na lista")
        check(opcoes[0].startswith("Escolha"), "nenhuma entrada vem marcada por engano")
        page.screenshot(path=f"{OUT}/1b-escolha-entrada.png")

        print("Início do áudio")
        escolhido = page.eval_on_selector_all(
            "#entradaSelect option", "els => els.filter(e => e.value)[0].value")
        page.select_option("#entradaSelect", escolhido)
        check(page.evaluate("() => !document.getElementById('entrarButton').disabled"),
              "o botão Entrar libera depois da escolha")
        page.click("#entrarButton")
        page.wait_for_timeout(2500)
        started = page.evaluate("() => document.getElementById('start').classList.contains('hidden')")
        check(started, "áudio iniciou e a tela inicial sumiu")
        note = page.evaluate("() => document.getElementById('startNote').textContent")
        if not started:
            print("   aviso:", note)
        page.screenshot(path=f"{OUT}/2-painel.png")

        state = page.evaluate("() => ({running: window.__aura.state.running, ctx: window.__aura.audio && window.__aura.audio.ctx.state})")
        check(state["running"] and state["ctx"] == "running", f"contexto de áudio rodando ({state})")

        print("Afinador reconhece a nota tocada")
        page.evaluate("() => window.__aura.toggle('tuner')")
        page.wait_for_timeout(2500)
        t = page.evaluate("() => ({hasNote: window.__aura.tuner.hasNote, note: window.__aura.tuner.note, hz: window.__aura.tuner.hz, cents: window.__aura.tuner.cents})")
        print(f"   nota {t['note']} ({t['hz']:.2f} Hz, {t['cents']:+.1f} cents)")
        check(t["hasNote"] and t["note"] == 28, "detectou E1 (nota MIDI 28)")
        check(abs(t["hz"] - 41.203) < 0.1, "frequência correta")
        page.screenshot(path=f"{OUT}/3-afinador.png")

        print("Saída muda com o afinador ligado")
        silent = page.evaluate("""() => new Promise((resolve) => {
            const a = window.__aura.audio, an = a.ctx.createAnalyser();
            an.fftSize = 2048; a.master.connect(an);
            setTimeout(() => {
                const buf = new Float32Array(an.fftSize); an.getFloatTimeDomainData(buf);
                a.master.disconnect(an);
                resolve(Math.max(...Array.from(buf, Math.abs)));
            }, 500);
        })""")
        check(silent < 1e-3, f"saída em silêncio (pico {silent:.2e})")

        page.evaluate("() => window.__aura.toggle('tuner')")
        page.wait_for_timeout(900)
        loud = page.evaluate("""() => new Promise((resolve) => {
            const a = window.__aura.audio, an = a.ctx.createAnalyser();
            an.fftSize = 2048; a.master.connect(an);
            setTimeout(() => {
                const buf = new Float32Array(an.fftSize); an.getFloatTimeDomainData(buf);
                a.master.disconnect(an);
                resolve(Math.max(...Array.from(buf, Math.abs)));
            }, 700);
        })""")
        print(f"   pico com o afinador desligado: {loud:.3f}")
        check(loud > 0.01, "som volta ao desligar o afinador")

        print("Controles respondem ao toque")
        box = page.locator("#panel").bounding_box()
        scale = page.evaluate("() => parseFloat(document.getElementById('panel').dataset.scale)")
        lay = page.evaluate("() => window.AuraLayout")
        lay_w, lay_h = lay["width"], lay["height"]
        rotated = page.evaluate("() => document.getElementById('panel').dataset.rotated === 'true'")
        def at(x, y):
            cx, cy = box["x"] + box["width"] / 2, box["y"] + box["height"] / 2
            ex, ey = (x - lay_w / 2) * scale, (y - lay_h / 2) * scale
            return (cx - ey, cy + ex) if rotated else (cx + ex, cy + ey)

        knob = lay["knobs"][2]            # LOW
        x, y = at(knob["cx"], knob["cy"])
        page.mouse.move(x, y)
        page.mouse.down()
        page.mouse.move(x, y - 60, steps=10)
        page.mouse.up()
        low = page.evaluate("() => window.__aura.state.params.low")
        print(f"   LOW após arrastar: {low:+.1f} dB")
        check(low > 2, "arrastar o knob aumenta o valor")

        page.mouse.dblclick(x, y)
        page.wait_for_timeout(200)
        check(abs(page.evaluate("() => window.__aura.state.params.low")) < 0.05, "toque duplo volta ao padrão")

        ex, ey = at(lay["driveHit"]["x"] + 60, lay["driveHit"]["y"] + 24)
        page.mouse.click(ex, ey)
        page.wait_for_timeout(200)
        check(page.evaluate("() => window.__aura.state.driveOn") is False, "footswitch ENGAGE desliga")
        page.mouse.click(ex, ey)
        page.wait_for_timeout(200)
        check(page.evaluate("() => window.__aura.state.driveOn") is True, "footswitch ENGAGE liga de novo")

        cx, cy = at(lay["cabHit"]["x"] + 24, lay["cabHit"]["y"] + 47)
        page.mouse.click(cx, cy)
        page.wait_for_timeout(300)
        check(page.evaluate("() => window.__aura.state.cab") is False, "chave do CAB desliga")
        page.mouse.click(cx, cy)
        page.wait_for_timeout(300)
        page.screenshot(path=f"{OUT}/4-controles.png")

        print("Menu")
        mx, my = at(lay["menuButton"]["x"] + 13, lay["menuButton"]["y"] + 11)
        page.mouse.click(mx, my)
        page.wait_for_timeout(500)
        check(page.is_visible("#menu"), "menu abre")
        check(page.locator("#menu button", has_text="Drive pesado").count() == 1, "presets no menu")
        sels = page.eval_on_selector_all(
            "#menu select",
            "els => els.map(e => [...e.options].map(o => o.textContent))")
        print(f"   seletores de dispositivo: {len(sels)}")
        for lista in sels:
            print("     " + ", ".join(lista))
        check(len(sels) == 2, "menu traz entrada e saída de áudio")
        check(any(len(l) > 1 for l in sels), "os dispositivos do sistema aparecem na lista")
        page.screenshot(path=f"{OUT}/5-menu.png")
        page.locator("#menu button", has_text="Drive pesado").click()
        page.wait_for_timeout(400)
        drive = page.evaluate("() => window.__aura.state.params.drive")
        check(abs(drive - 10) < 0.05, f"preset aplicado (drive {drive:+.1f} dB)")

        print("Estado salvo no aparelho")
        page.reload(wait_until="networkidle")
        page.wait_for_timeout(800)
        check(abs(page.evaluate("() => window.__aura.state.params.drive") - 10) < 0.05, "valores voltam após recarregar")

        print("Telas pequenas (celular em pé e deitado)")
        page.click("#startButton")           # religa o áudio depois do recarregamento
        page.wait_for_timeout(2500)
        check(page.evaluate("() => document.getElementById('start').classList.contains('hidden')"),
              "com a entrada já escolhida, entra direto sem perguntar de novo")
        for name, w, h in [("celular-deitado", 844, 390), ("celular-em-pe", 390, 844), ("tablet", 1024, 768)]:
            page.set_viewport_size({"width": w, "height": h})
            page.wait_for_timeout(400)
            info = page.evaluate("""() => {
                const el = document.getElementById('panel');
                const r = el.getBoundingClientRect();
                return { w: r.width, h: r.height, rotated: el.dataset.rotated === 'true',
                         // Precisa caber E estar visível: a caixa inteira dentro da tela
                         inside: r.left >= -1 && r.top >= -1
                                 && r.right <= window.innerWidth + 1
                                 && r.bottom <= window.innerHeight + 1,
                         offCentre: Math.max(
                             Math.abs((r.left + r.right) / 2 - window.innerWidth / 2),
                             Math.abs((r.top + r.bottom) / 2 - window.innerHeight / 2)) };
            }""")
            filled = max(info["w"], info["h"]) / max(w, h)
            print(f"   {name}: painel {info['w']:.0f}x{info['h']:.0f}, girado={info['rotated']}, "
                  f"ocupa {filled*100:.0f}% do lado maior, centro desviado {info['offCentre']:.0f}px")
            check(info["inside"] and filled > 0.6 and info["offCentre"] < 4,
                  f"painel grande, centralizado e inteiro na tela ({name}: {w}x{h})")
            page.screenshot(path=f"{OUT}/6-{name}.png")

        print("Controles com o painel girado (celular em pé)")
        page.set_viewport_size({"width": 390, "height": 844})
        page.wait_for_timeout(400)
        box = page.locator("#panel").bounding_box()
        scale = page.evaluate("() => parseFloat(document.getElementById('panel').dataset.scale)")
        rotated = page.evaluate("() => document.getElementById('panel').dataset.rotated === 'true'")
        check(rotated, "painel gira no celular em pé")
        page.evaluate("() => window.__aura.setParam('mid', 0)")
        k = lay["knobs"][3]                  # MID
        x, y = at(k["cx"], k["cy"])
        page.mouse.move(x, y)
        page.mouse.down()
        page.mouse.move(x + 60, y, steps=10)   # girado: arrastar para a direita aumenta
        page.mouse.up()
        mid = page.evaluate("() => window.__aura.state.params.mid")
        print(f"   MID após arrastar: {mid:+.1f} dB")
        check(mid > 2, "arrastar funciona com o painel girado")
        ex, ey = at(lay["tunerHit"]["x"] + 60, lay["tunerHit"]["y"] + 24)
        page.mouse.click(ex, ey)
        page.wait_for_timeout(1500)
        check(page.evaluate("() => window.__aura.state.tuner") is True, "footswitch TUNER funciona girado")
        page.screenshot(path=f"{OUT}/8-celular-afinador.png")
        page.mouse.click(ex, ey)
        page.wait_for_timeout(300)
        page.set_viewport_size({"width": 1280, "height": 720})
        page.wait_for_timeout(300)

        print("Funciona sem internet")
        page.set_viewport_size({"width": 1280, "height": 720})
        page.wait_for_timeout(300)
        registered = page.evaluate("() => navigator.serviceWorker.ready.then(() => true).catch(() => false)")
        check(registered is True, "service worker registrado")
        page.context.set_offline(True)
        page.reload(wait_until="domcontentloaded")
        page.wait_for_timeout(1200)
        offline_ok = page.evaluate("() => !!document.getElementById('startButton') && !!window.AuraLayout")
        check(offline_ok, "página abre offline")
        page.screenshot(path=f"{OUT}/7-offline.png")
        page.context.set_offline(False)

        check(not errors, f"sem erros no console ao final: {errors[:3]}")
        browser.close()
    server.shutdown()

    print("\nTodos os testes passaram." if not failures else f"\n{len(failures)} teste(s) falharam.")
    return 0 if not failures else 1


if __name__ == "__main__":
    sys.exit(main())
