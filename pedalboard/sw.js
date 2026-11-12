// CORTES Pedalboard — service worker: guarda o app para funcionar sem internet.
// Copyright (c) 2026 Thiago Galvão. Todos os direitos reservados.
const CACHE = 'cortes-pedalboard-v1';
const FILES = [
  './', 'index.html', 'style.css', 'app.js', 'engine.js', 'presets.js', 'pb-processor.js',
  'manifest.webmanifest', 'assets/cab_4x10.wav', 'assets/icon-192.png', 'assets/icon-512.png',
];
self.addEventListener('install', (e) => {
  e.waitUntil(caches.open(CACHE).then((c) => c.addAll(FILES)).then(() => self.skipWaiting()));
});
self.addEventListener('activate', (e) => {
  e.waitUntil(caches.keys()
    .then((ks) => Promise.all(ks.filter((k) => k !== CACHE).map((k) => caches.delete(k))))
    .then(() => self.clients.claim()));
});
self.addEventListener('fetch', (e) => {
  if (e.request.method !== 'GET') return;
  e.respondWith(
    fetch(e.request)
      .then((res) => {
        if (res && res.ok && new URL(e.request.url).origin === location.origin)
          caches.open(CACHE).then((c) => c.put(e.request, res.clone()));
        return res;
      })
      .catch(() => caches.match(e.request).then((hit) => hit || caches.match('index.html')))
  );
});
