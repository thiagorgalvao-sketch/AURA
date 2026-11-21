// AURA Preamp Bass — service worker da versão web
// Copyright (c) 2026 Thiago Galvão. Todos os direitos reservados.
// Obra protegida pelas Leis 9.609/1998 e 9.610/1998. Uso pessoal permitido;
// cópia, redistribuição ou obra derivada apenas com autorização por escrito.
// Licença: ver LICENSE  |  https://github.com/ledixplugins/Drive
// AURA Preamp Bass — service worker: guarda o app para funcionar sem internet.
const CACHE = 'aura-preamp-bass-v1-2d6f9fe';
const FILES = [
  './', 'index.html', 'style.css', 'app.js', 'layout.js', 'tuner.js', 'aura-processor.js',
  'manifest.webmanifest',
  'assets/background.jpg', 'assets/arcs_lit.png', 'assets/pointer.png', 'assets/knob.png',
  'assets/led_drive_on.png', 'assets/led_tuner_on.png', 'assets/led_cab_on.png',
  'assets/cab_off.png', 'assets/footswitch_down.png', 'assets/tuner_down.png',
  'assets/cab_4x10.wav', 'assets/cab_1x15.wav',
  'assets/Roboto.ttf', 'assets/RussoOne-Regular.ttf',
  'assets/icon-192.png', 'assets/icon-512.png',
];

self.addEventListener('install', (e) => {
  e.waitUntil(caches.open(CACHE).then((c) => c.addAll(FILES)).then(() => self.skipWaiting()));
});

self.addEventListener('activate', (e) => {
  e.waitUntil(caches.keys()
    .then((keys) => Promise.all(keys.filter((k) => k !== CACHE).map((k) => caches.delete(k))))
    .then(() => self.clients.claim()));
});

// Rede primeiro (para pegar versões novas), com o cache como reserva
self.addEventListener('fetch', (e) => {
  if (e.request.method !== 'GET') return;
  e.respondWith(
    fetch(e.request, { cache: 'reload' })
      .then((res) => {
        if (res && res.ok && new URL(e.request.url).origin === location.origin)
          caches.open(CACHE).then((c) => c.put(e.request, res.clone()));
        return res;
      })
      .catch(() => caches.match(e.request, { ignoreSearch: true }).then((hit) => {
        // Os arquivos ficam no cache sem "?v=...", mas o HTML pede com a
        // versão — ignorar a query no match evita cair no index.html por
        // engano (e quebrar um .js/.css virando HTML quando offline).
        if (hit) return hit;
        if (e.request.mode === 'navigate') return caches.match('index.html');
        return Response.error();
      }))
  );
});
