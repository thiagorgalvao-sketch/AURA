#!/usr/bin/env python3
"""
Monta as imagens da interface do AURA Bass a partir da arte de referência.

Entradas (Resources/mockup):
  layout.jpg          painel completo (2x), sem marca
  plate_empty.jpg     a mesma placa vazia, perfeitamente alinhada com layout.jpg
  generic_print.png   textos impressos genéricos (texto vertical, subtítulo do rodapé, microtextos)
Fontes (Resources/fonts): RussoOne-Regular.ttf, Roboto.ttf

Saídas (Resources/ui), todas em 2x:
  background.jpg      painel sem os indicadores dos knobs e sem os valores, LEDs apagados,
                      chave do CAB ligada, com o nome AURA BASS
  pointer.png         indicador do knob (gira na interface), centrado no quadro
  led_drive_on.png    LED azul aceso (sobreposição)
  footswitch_down.png footswitch do DRIVE pressionado (sobreposição)
  led_cab_on.png      LED laranja aceso (sobreposição)
  cab_off.png         chave do CAB desligada (alavanca espelhada, sobreposição)

As coordenadas abaixo estão em pixels da imagem de referência (1600x1194, tela Retina).
O recorte do painel começa em CROP_X, CROP_Y; dividir por 2 dá as unidades de Source/Layout.h.

Uso: python3 Tools/build_ui_assets.py
"""
import math
import os
import numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MOCK = os.path.join(ROOT, "Resources", "mockup")
FONTS = os.path.join(ROOT, "Resources", "fonts")
OUT = os.path.join(ROOT, "Resources", "ui")

CROP_X, CROP_Y, CROP_W, CROP_H = 74, 262, 1452, 678

# Knobs: centro e ângulo atual do indicador (graus, 0 = para cima, horário)
KNOBS = [(217.0, 647.0, 352), (392.5, 651.0, 358), (569.5, 647.0, 358), (746.0, 647.0, 0),
         (923.0, 647.0, 0), (1101.0, 647.5, 358), (1276.0, 646.5, 132)]
POINTER_R0, POINTER_R1, POINTER_HALF_W = 5.0, 50.0, 7.5
REF_KNOB = 3                              # MID: indicador exatamente para cima

VALUE_RECTS = [(150, 745, 300, 790), (325, 745, 475, 790), (500, 745, 650, 790), (675, 745, 825, 790),
               (850, 745, 1000, 790), (1020, 745, 1180, 790), (1195, 745, 1350, 790)]

LED_DRIVE = (249.0, 445.0, 9.5)           # x, y, raio da lente (posição original na arte)

# Footswitch do DRIVE (renderizado por Tools/FootswitchRender) e nova posição do LED azul
FS_CENTER = (190.0, 452.0)
FS_WIPE = (145, 398, 352, 494)            # região do antigo botão power + LED + rótulo
FS_DOWN_HALF = 48
LED_DRIVE_NEW = (266.0, 452.0)
LED_DRIVE_HALF = 22
LED_CAB = (1416.7, 579.3, 9.0)
LED_HALF = 32                             # meio lado do recorte do LED (px)

TOGGLE_AXIS_X = 1417.0                    # eixo vertical da chave (para espelhar)
TOGGLE_CENTER = (1417.0, 652.0)
TOGGLE_HALF = 50

POINTER_HALF = 48                         # meio lado do quadro do indicador (px)


def load(name):
    return np.asarray(Image.open(os.path.join(MOCK, name)).convert("RGB")).astype(np.float32)


def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def bilinear(img, xs, ys):
    h, w = img.shape[:2]
    xs = np.clip(xs, 0, w - 2)
    ys = np.clip(ys, 0, h - 2)
    x0 = np.floor(xs).astype(int)
    y0 = np.floor(ys).astype(int)
    tx = (xs - x0)[..., None]
    ty = (ys - y0)[..., None]
    a = img[y0, x0]
    b = img[y0, x0 + 1]
    c = img[y0 + 1, x0]
    d = img[y0 + 1, x0 + 1]
    return (a * (1 - tx) + b * tx) * (1 - ty) + (c * (1 - tx) + d * tx) * ty


def lum(img):
    return img[..., 0] * 0.299 + img[..., 1] * 0.587 + img[..., 2] * 0.114


def pointer_weight(cx, cy, deg, xs, ys):
    a = math.radians(deg)
    ux, uy = math.sin(a), -math.cos(a)
    vx, vy = xs - cx, ys - cy
    along = vx * ux + vy * uy
    perp = np.abs(vx * uy - vy * ux)
    return (smoothstep(POINTER_HALF_W, POINTER_HALF_W - 2.0, perp)
            * smoothstep(POINTER_R0, POINTER_R0 + 2.0, along)
            * smoothstep(POINTER_R1, POINTER_R1 - 2.0, along))


def remove_pointer(img, cx, cy, deg):
    """Preenche o indicador interpolando ao longo do anel (mesmo raio), dos dois lados da ranhura.
    O alumínio torneado varia suavemente em volta do círculo, então o preenchimento some."""
    x0, y0 = int(cx - 60), int(cy - 60)
    ys, xs = np.mgrid[y0:y0 + 120, x0:x0 + 120].astype(np.float32)
    w = pointer_weight(cx, cy, deg, xs, ys)[..., None]
    patch = img[y0:y0 + 120, x0:x0 + 120].copy()

    vx, vy = xs - cx, ys - cy
    r = np.maximum(np.sqrt(vx * vx + vy * vy), 1.0)
    phi = np.arctan2(vx, -vy)                                 # 0 = para cima, horário
    phi0 = math.radians(deg)
    dphi = np.arctan2(np.sin(phi - phi0), np.cos(phi - phi0))
    half = (POINTER_HALF_W + 1.5) / r                         # meia largura angular da ranhura
    pa, pb = phi0 - half, phi0 + half
    side_a = bilinear(img, cx + r * np.sin(pa), cy - r * np.cos(pa))
    side_b = bilinear(img, cx + r * np.sin(pb), cy - r * np.cos(pb))
    t = np.clip((dphi + half) / (2 * half), 0, 1)[..., None]
    interp = side_a * (1 - t) + side_b * t
    filled = patch * (1 - w) + interp * w
    img[y0:y0 + 120, x0:x0 + 120] = filled
    return patch, filled, w[..., 0], (x0, y0)


def erase_text(img, plate, rect, threshold=22.0):
    """Remove texto claro trocando-o pela placa vazia, preservando sombras locais."""
    x0, y0, x1, y1 = rect
    b = img[y0:y1, x0:x1]
    a = plate[y0:y1, x0:x1]
    mask = (lum(b) - lum(a)) > threshold
    mask = np.asarray(Image.fromarray((mask * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(5))) > 0
    valid = (~mask).astype(np.float32)
    ratio = np.where(valid[..., None] > 0, b / np.maximum(a, 1.0), 0.0)

    def blur(arr, sigma=6.0):
        k = np.exp(-0.5 * (np.arange(-18, 19) / sigma) ** 2)
        k /= k.sum()
        pad = np.pad(arr.astype(np.float32), 18, mode="edge")
        pad = np.apply_along_axis(lambda m: np.convolve(m, k, mode="same"), 0, pad)
        pad = np.apply_along_axis(lambda m: np.convolve(m, k, mode="same"), 1, pad)
        return pad[18:-18, 18:-18]

    num = np.stack([blur(ratio[..., i] * valid) for i in range(3)], -1)
    den = blur(valid)[..., None]
    r = num / np.maximum(den, 1e-3)
    fill = a * np.clip(r, 0.5, 1.6)
    soft = np.asarray(Image.fromarray((mask * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(1.0))).astype(np.float32)[..., None] / 255.0
    img[y0:y1, x0:x1] = b * (1 - soft) + fill * soft


def led_off(img, plate, x, y, r, tint):
    """Apaga o LED: tira o brilho (volta à placa vazia) e escurece a lente."""
    R = LED_HALF
    x0, y0 = int(round(x)) - R, int(round(y)) - R
    ys, xs = np.mgrid[y0:y0 + 2 * R, x0:x0 + 2 * R].astype(np.float32)
    d = np.sqrt((xs - x) ** 2 + (ys - y) ** 2)
    on = img[y0:y0 + 2 * R, x0:x0 + 2 * R]
    base = plate[y0:y0 + 2 * R, x0:x0 + 2 * R]
    l = lum(on)[..., None] / 255.0
    lens = np.array(tint, np.float32) * (0.10 + 0.25 * l ** 2) + 10.0
    # reflexo especular pequeno no alto à esquerda
    spec = np.exp(-(((xs - (x - r * 0.35)) ** 2 + (ys - (y - r * 0.4)) ** 2) / (2 * (r * 0.22) ** 2)))[..., None]
    lens = lens + spec * 120.0
    rim = smoothstep(r - 0.6, r + 1.2, d)[..., None]
    ring = np.exp(-((d - r - 0.8) ** 2) / 1.2)[..., None] * 45.0
    off = lens * (1 - rim) + (base + ring) * rim
    edge = smoothstep(R - 2, R - 8, d)[..., None]
    img[y0:y0 + 2 * R, x0:x0 + 2 * R] = on * (1 - edge) + off * edge


def crop_rgba(img, cx, cy, half, alpha=None):
    x0, y0 = int(round(cx)) - half, int(round(cy)) - half
    rgb = np.clip(img[y0:y0 + 2 * half, x0:x0 + 2 * half], 0, 255).astype(np.uint8)
    if alpha is None:
        ys, xs = np.mgrid[0:2 * half, 0:2 * half].astype(np.float32)
        d = np.sqrt((xs + 0.5 - half) ** 2 + (ys + 0.5 - half) ** 2)
        alpha = smoothstep(half - 1, half - 7, d)
    a = (np.clip(alpha, 0, 1) * 255).astype(np.uint8)
    return Image.fromarray(np.dstack([rgb, a]), "RGBA")


def draw_text(canvas, text, font, x, baseline, colour, anchor="ls", spacing=0.0):
    """Desenha texto com espaçamento entre letras; anchor 'ls' = esquerda/base, 'rs' = direita/base."""
    d = ImageDraw.Draw(canvas)
    widths = [font.getlength(ch) for ch in text]
    total = sum(widths) + spacing * (len(text) - 1)
    cx = x - total if anchor == "rs" else x
    for ch, w in zip(text, widths):
        d.text((cx, baseline), ch, font=font, fill=colour, anchor="ls")
        cx += w + spacing


def distress(alpha, seed=7):
    rng = np.random.default_rng(seed)
    h, w = alpha.shape
    fine = np.asarray(Image.fromarray((rng.random((h // 2 + 1, w // 2 + 1)) * 255).astype(np.uint8)).resize((w, h), Image.BILINEAR)).astype(np.float32) / 255
    coarse = np.asarray(Image.fromarray((rng.random((h // 18 + 1, w // 18 + 1)) * 255).astype(np.uint8)).resize((w, h), Image.BICUBIC)).astype(np.float32) / 255
    specks = smoothstep(0.78, 0.86, fine) * smoothstep(0.50, 0.70, coarse)
    pits = (rng.random((h, w)) > 0.9965).astype(np.float32)
    pits = np.asarray(Image.fromarray((pits * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(3))).astype(np.float32) / 255
    return alpha * np.clip(1.0 - specks * 0.9 - pits * 0.8, 0, 1)


def composite(img, rgba_canvas, colour, alpha_scale=1.0, use_distress=False, seed=7):
    a = np.asarray(rgba_canvas).astype(np.float32) / 255.0
    if use_distress:
        a = distress(a, seed)
    a = (a * alpha_scale)[..., None]
    img[:] = img * (1 - a) + np.array(colour, np.float32) * a


def main():
    os.makedirs(OUT, exist_ok=True)
    layout = load("layout.jpg")
    plate = load("plate_empty.jpg")
    img = layout.copy()
    H, W = img.shape[:2]

    # 1) Indicadores dos knobs: remove todos e guarda o do MID como sprite
    pointer_sprite = None
    for i, (cx, cy, deg) in enumerate(KNOBS):
        patch, filled, w, (x0, y0) = remove_pointer(img, cx, cy, deg)
        if i == REF_KNOB:
            lo, lf = lum(patch), lum(filled)
            alpha = np.clip((lf - lo) / np.maximum(lf, 1.0) * 1.8, 0, 1) * w
            ox, oy = int(cx) - POINTER_HALF - x0, int(cy) - POINTER_HALF - y0
            rgb = np.clip(patch, 0, 255)[oy:oy + 2 * POINTER_HALF, ox:ox + 2 * POINTER_HALF].astype(np.uint8)
            a = (alpha[oy:oy + 2 * POINTER_HALF, ox:ox + 2 * POINTER_HALF] * 255).astype(np.uint8)
            pointer_sprite = Image.fromarray(np.dstack([rgb, a]), "RGBA")
            # recentra pelo centro fracionário do knob
            fx, fy = cx - int(cx), cy - int(cy)
            pointer_sprite = pointer_sprite.transform(pointer_sprite.size, Image.AFFINE, (1, 0, fx, 0, 1, fy), resample=Image.BICUBIC)

    # 2) Valores: saem da imagem (a interface desenha ao vivo)
    for r in VALUE_RECTS:
        erase_text(img, plate, r)

    # 3) LED do CAB: aceso (sobreposição) e apagado no fundo
    led_cab_on = crop_rgba(layout, LED_CAB[0], LED_CAB[1], LED_HALF)
    led_off(img, plate, *LED_CAB, tint=(200, 100, 30))

    # 3b) Botão DRIVE: troca o botão power por um footswitch de pisar
    def radial_alpha(half, inner, outer):
        ys, xs = np.mgrid[0:2 * half, 0:2 * half].astype(np.float32)
        d = np.sqrt((xs + 0.5 - half) ** 2 + (ys + 0.5 - half) ** 2)
        return smoothstep(outer, inner, d)[..., None]

    def paste(dst, patch, cx, cy, alpha):
        h = patch.shape[0] // 2
        x0, y0 = int(round(cx)) - h, int(round(cy)) - h
        dst[y0:y0 + 2 * h, x0:x0 + 2 * h] = dst[y0:y0 + 2 * h, x0:x0 + 2 * h] * (1 - alpha) + patch * alpha

    def crop(src, cx, cy, half):
        x0, y0 = int(round(cx)) - half, int(round(cy)) - half
        return src[y0:y0 + 2 * half, x0:x0 + 2 * half].copy()

    # LED azul: aceso (da arte) e apagado (sintetizado), movidos para a nova posição
    led_on_patch = crop(layout, LED_DRIVE[0], LED_DRIVE[1], LED_DRIVE_HALF)
    tmp = layout.copy()
    led_off(tmp, plate, *LED_DRIVE, tint=(40, 90, 200))
    led_off_patch = crop(tmp, LED_DRIVE[0], LED_DRIVE[1], LED_DRIVE_HALF)

    # Apaga o botão antigo (volta à placa vazia, com borda suave)
    x0, y0, x1, y1 = FS_WIPE
    ys, xs = np.mgrid[y0:y1, x0:x1].astype(np.float32)
    edge = np.minimum.reduce([xs - x0, x1 - 1 - xs, ys - y0, y1 - 1 - ys])
    wa = smoothstep(0, 10, edge)[..., None]
    img[y0:y1, x0:x1] = img[y0:y1, x0:x1] * (1 - wa) + plate[y0:y1, x0:x1] * wa

    # Sombra do footswitch na placa
    fs_up = np.asarray(Image.open(os.path.join(MOCK, "footswitch_up.png")).convert("RGBA")).astype(np.float32)
    fs_down = np.asarray(Image.open(os.path.join(MOCK, "footswitch_down.png")).convert("RGBA")).astype(np.float32)
    fs_shadow = np.asarray(Image.open(os.path.join(MOCK, "footswitch_shadow.png")).convert("L")).astype(np.float32) / 255.0
    fh = fs_up.shape[0] // 2
    a_up = fs_up[..., 3] / 255.0
    drop = np.roll(a_up, 7, axis=0)
    drop = np.asarray(Image.fromarray((drop * 255).astype(np.uint8)).filter(ImageFilter.GaussianBlur(7))).astype(np.float32) / 255.0
    mul = (fs_shadow ** 1.6) * (1.0 - 0.45 * drop)
    fx0, fy0 = int(FS_CENTER[0]) - fh, int(FS_CENTER[1]) - fh
    img[fy0:fy0 + 2 * fh, fx0:fx0 + 2 * fh] *= mul[..., None]
    fs_base = img.copy()
    paste(img, fs_up[..., :3], FS_CENTER[0], FS_CENTER[1], fs_up[..., 3:4] / 255.0)
    paste(img, led_off_patch, LED_DRIVE_NEW[0], LED_DRIVE_NEW[1], radial_alpha(LED_DRIVE_HALF, 13, 21))

    # 4) Textos impressos genéricos da primeira referência
    gp = np.asarray(Image.open(os.path.join(MOCK, "generic_print.png")).convert("RGBA")).astype(np.float32)
    gx, gy = 1200, 340
    gh, gw = gp.shape[:2]
    ga = gp[..., 3:4] / 255.0
    img[gy:gy + gh, gx:gx + gw] = img[gy:gy + gh, gx:gx + gw] * (1 - ga) + gp[..., :3] * ga

    # 5) Marca AURA BASS no estilo da referência
    russo = lambda s: ImageFont.truetype(os.path.join(FONTS, "RussoOne-Regular.ttf"), s)
    roboto = lambda s, wght=500: ImageFont.truetype(os.path.join(FONTS, "Roboto.ttf"), s)

    def text_layer(text, font, x, baseline, anchor="ls", spacing=0.0):
        layer = Image.new("L", (W, H), 0)
        d = ImageDraw.Draw(layer)
        widths = [font.getlength(ch) for ch in text]
        total = sum(widths) + spacing * (len(text) - 1)
        cx = x - total if anchor == "rs" else x
        for ch, wch in zip(text, widths):
            d.text((cx, baseline), ch, font=font, fill=255, anchor="ls")
            cx += wch + spacing
        return layer

    # Logotipo grande (com desgaste)
    composite(img, text_layer("AURA BASS", russo(77), 163, 380, spacing=1.5), (218, 219, 223), 0.97, True, 11)
    # Selo superior direito
    composite(img, text_layer("AURA BASS", russo(33), 1438, 357, "rs", 0.5), (142, 144, 149), 1.0)
    composite(img, text_layer("BASS PREAMP", roboto(18), 1437, 379, "rs", 0.6), (128, 130, 135), 1.0)
    # Rodapé inferior direito
    composite(img, text_layer("AURA BASS", russo(31), 1440, 888, "rs", 0.5), (128, 130, 135), 1.0)
    # Rótulo do botão DRIVE
    composite(img, text_layer("DRIVE", roboto(19), 293, 459, "ls", 1.8), (170, 172, 177), 1.0)

    # Sobreposições do DRIVE: LED aceso e footswitch pressionado
    img_on = img.copy()
    paste(img_on, led_on_patch, LED_DRIVE_NEW[0], LED_DRIVE_NEW[1], radial_alpha(LED_DRIVE_HALF, 13, 21))
    led_drive_on = crop_rgba(img_on, LED_DRIVE_NEW[0], LED_DRIVE_NEW[1], LED_DRIVE_HALF)
    img_down = fs_base.copy()
    down_rgb = fs_down[..., :3] * 0.94      # um pouco mais escuro: afundado
    paste(img_down, down_rgb, FS_CENTER[0], FS_CENTER[1], fs_down[..., 3:4] / 255.0)
    footswitch_down = crop_rgba(img_down, FS_CENTER[0], FS_CENTER[1], FS_DOWN_HALF, np.ones((2 * FS_DOWN_HALF, 2 * FS_DOWN_HALF), np.float32))

    # 6) Chave do CAB desligada: alavanca espelhada (chave de dois lados)
    tx, ty = TOGGLE_CENTER
    x0, y0 = int(tx) - TOGGLE_HALF, int(ty) - TOGGLE_HALF
    ys, xs = np.mgrid[y0:y0 + 2 * TOGGLE_HALF, x0:x0 + 2 * TOGGLE_HALF].astype(np.float32)
    mirrored = bilinear(img, 2 * TOGGLE_AXIS_X - xs, ys)
    d = np.sqrt((xs - tx) ** 2 + (ys - ty) ** 2)
    alpha = smoothstep(TOGGLE_HALF - 1, TOGGLE_HALF - 9, d)
    cab_off = Image.fromarray(np.dstack([np.clip(mirrored, 0, 255).astype(np.uint8), (alpha * 255).astype(np.uint8)]), "RGBA")

    # 7) Salva (recortando o painel)
    bg = Image.fromarray(np.clip(img, 0, 255).astype(np.uint8)).crop((CROP_X, CROP_Y, CROP_X + CROP_W, CROP_Y + CROP_H))
    bg.save(os.path.join(OUT, "background.jpg"), quality=95, subsampling=0)
    pointer_sprite.save(os.path.join(OUT, "pointer.png"))
    led_drive_on.save(os.path.join(OUT, "led_drive_on.png"))
    led_cab_on.save(os.path.join(OUT, "led_cab_on.png"))
    cab_off.save(os.path.join(OUT, "cab_off.png"))
    footswitch_down.save(os.path.join(OUT, "footswitch_down.png"))

    # Prévia com as sobreposições "ligadas" para conferência
    print("background", bg.size)


if __name__ == "__main__":
    main()
