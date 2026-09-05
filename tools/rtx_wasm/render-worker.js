// RetroTrax Web-Player: WASM-Rendering in einem eigenen Worker.
//
// Vorher lief die ganze Klang-Erzeugung (Modul laden, Haeppchen rendern,
// Tempo-Antest, Komplett-Vorrender) auf dem Haupt-Thread - bei rechenlastigen
// Songs auf schwachen Geraeten konnte das den Tab kurz einfrieren (Klick auf
// Knoepfe reagiert erst verzoegert, Scope-Anzeige ruckelt). Hier drin passiert
// jetzt ALLES, was das WASM-Modul beruehrt; der Haupt-Thread (player.html)
// schickt nur noch kleine Kommandos rein und bekommt fertige Float32Array-
// Haeppchen zurueck (per postMessage-Transfer, ohne Kopieren).
//
// Nachrichtenformat Haupt-Thread -> Worker: { id, cmd, args }
// Antworten Worker -> Haupt-Thread:
//   Zwischenstand:  { id, type:'progress', p }
//   Endergebnis:    { id, type:'done', ok:true,  result }
//                    { id, type:'done', ok:false, error }
importScripts('rtx_wasm.js');

let Module = null;
let handle = null;       // aktuell offener Stream-Handle (nur einer gleichzeitig, wie bisher)
let streamKind = null;   // 'rtx' | 'tfmx'
let curSr = 44100;
let tfmxRendered = 0;    // Bookkeeping fuer die TFMX-Spieldauer-Deckelung (die Engine laeuft endlos)
const TFMX_SECONDS = 120;
const CHUNK_FRAMES = 8192;

async function ensureModule () {
  if (!Module) Module = await RtxModule();
  return Module;
}

// ── Format-Erkennung + Laden (identisch zur alten Logik in player.html) ──
const txt = (b, off, len) => String.fromCharCode(...b.subarray(off, off + len));
const isTfmx = (b) => b.length >= 4 && b[0]===0x54 && b[1]===0x46 && b[2]===0x4D && b[3]===0x58; // "TFMX"
const isRtx  = (b) => b.length >= 4 && b[0]===0x52 && b[1]===0x54 && b[2]===0x58 && b[3]===0x31; // "RTX1"
const MOD_TAGS = ['M.K.','M!K!','FLT4','FLT8','OCTA','CD81','4CHN','6CHN','8CHN'];
function modKind (b) {
  if (b.length >= 17 && txt(b, 0, 17) === 'Extended Module: ') return 2;   // XM
  if (b.length >= 48 && txt(b, 44, 4) === 'SCRM')              return 3;   // S3M
  if (b.length >=  4 && txt(b, 0, 4)  === 'IMPM')              return 4;   // IT
  if (b.length >= 1084 && (MOD_TAGS.includes(txt(b, 1080, 4))
      || /^[0-9]{1,2}CH[N]?$/.test(txt(b, 1080, 4))))          return 1;   // MOD
  return 0;
}
const KIND_NAME = { 1:'MOD', 2:'XM', 3:'S3M', 4:'IT' };

function loadModBytes (h, bytes, kind) {
  try { Module.FS.mkdir('/work'); } catch (e) {}
  Module.FS.writeFile('/work/song.mod', bytes);
  return Module.ccall('rtx_load_mod', 'number', ['number','string','number'], [h, '/work/song.mod', kind]);
}
function loadPackedBytes (h, bytes) {
  const p = Module._malloc(bytes.length);
  Module.HEAPU8.set(bytes, p);
  const n = Module.ccall('rtx_load_rtx', 'number', ['number','number','number'], [h, p, bytes.length]);
  Module._free(p);
  return n;
}

// Laedt eine Datei-Liste (bufs: [{name, bytes}]) auf einen frischen Handle.
// Gemeinsamer Weg fuer probe/open/renderFull - wirft bei Formatfehlern.
function loadOnHandle (h, bufs) {
  const mdat = bufs.find(b => isTfmx(b.bytes));
  if (mdat) {
    const smpl = bufs.find(b => b !== mdat);
    if (!smpl) throw new Error('TFMX braucht ZWEI Dateien: mdat.* UND smpl.* (beide zusammen auswählen).');
    try { Module.FS.mkdir('/work'); } catch (e) {}
    Module.FS.writeFile('/work/mdat.song', mdat.bytes);
    Module.FS.writeFile('/work/smpl.song', smpl.bytes);
    return { kind:'tfmx', label: mdat.name, info: 'TFMX' };
  }
  const b = bufs[0].bytes;
  const packed = isRtx(b), mk = packed ? 0 : modKind(b);
  let nInst;
  if (packed)   nInst = loadPackedBytes(h, b);
  else if (mk)  nInst = loadModBytes(h, b, mk);
  else          nInst = Module.ccall('rtx_load_retrotrax', 'number', ['number','string'],
                                     [h, new TextDecoder().decode(b)]);
  if (nInst < 0) throw new Error('Datei nicht lesbar (.retrotrax, .rtx, .mod, .xm, .s3m oder .it erwartet).');
  return { kind:'rtx', label: bufs[0].name,
           info: nInst + (mk ? ' Sample(s), ' + KIND_NAME[mk] : ' Instrument(e)' + (packed ? ', gepackt' : '')) };
}

function readChunk (h, got) {
  const ptr = Module.ccall('rtx_buffer', 'number', ['number'], [h]);
  const base = ptr >> 2, heap = Module.HEAPF32;   // HEAPF32 nach Speicherwachstum frisch holen
  const l = new Float32Array(got), r = new Float32Array(got);
  for (let i = 0; i < got; i++) { l[i] = heap[base + 2*i]; r[i] = heap[base + 2*i + 1]; }
  return { l, r };
}

function destroyHandle () {
  if (handle != null) { Module.ccall('rtx_destroy', null, ['number'], [handle]); handle = null; }
  streamKind = null; tfmxRendered = 0;
}

// ── Kommandos ─────────────────────────────────────────────────────────────
const handlers = {
  async init () {
    await ensureModule();
    return {};
  },

  // Tempo-Antest: kann DIESES Geraet DIESEN Song in Echtzeit streamen?
  async probe ({ bufs, sr }) {
    await ensureModule();
    const h = Module.ccall('rtx_create', 'number', ['number'], [sr]);
    try {
      const meta = loadOnHandle(h, bufs);
      if (meta.kind === 'tfmx') return { ok: true };   // TFMX bleibt unveraendert (eigener, gedeckelter Weg)
      Module.ccall('rtx_stream_start', 'number', ['number'], [h]);
      const PROBE_CHUNKS = 6, PROBE_SAFETY = 0.6;
      const chunkMs = (CHUNK_FRAMES / sr) * 1000;
      let worstMs = 0;
      for (let i = 0; i < PROBE_CHUNKS; i++) {
        const t0 = performance.now();
        const got = Module.ccall('rtx_stream_render', 'number', ['number','number'], [h, CHUNK_FRAMES]);
        const ms = performance.now() - t0;
        if (got <= 0) break;
        if (i > 0) worstMs = Math.max(worstMs, ms);  // erstes Haeppchen (Anlauf/JIT) nicht werten
      }
      return { ok: worstMs < chunkMs * PROBE_SAFETY };
    } catch (e) {
      return { ok: true };                            // im Zweifel Streaming versuchen
    } finally {
      Module.ccall('rtx_destroy', null, ['number'], [h]);
    }
  },

  // Song laden und Stream starten (spielt noch nicht - erstes render() holt Ton).
  async open ({ bufs, sr }) {
    await ensureModule();
    destroyHandle();
    const h = Module.ccall('rtx_create', 'number', ['number'], [sr]);
    try {
      const meta = loadOnHandle(h, bufs);
      if (meta.kind === 'tfmx') {
        const rc = Module.ccall('rtx_stream_start_tfmx', 'number', ['number','string'], [h, '/work/mdat.song']);
        if (rc < 0) throw new Error('TFMX nicht abspielbar (fehlt die zweite Datei mdat/smpl?).');
        handle = h; streamKind = 'tfmx'; curSr = sr; tfmxRendered = 0;
        return { kind:'tfmx', label: meta.label, info: meta.info, duration: TFMX_SECONDS, durationExact: true };
      }
      Module.ccall('rtx_stream_start', 'number', ['number'], [h]);
      const duration = Module.ccall('rtx_estimate_seconds', 'number', ['number'], [h]);
      handle = h; streamKind = 'rtx'; curSr = sr;
      return { kind:'rtx', label: meta.label, info: meta.info, duration, durationExact: false };
    } catch (e) {
      Module.ccall('rtx_destroy', null, ['number'], [h]);
      throw e;
    }
  },

  // Naechstes Haeppchen des offenen Streams rendern.
  async render ({ frames }) {
    if (handle == null) throw new Error('kein offener Stream');
    if (streamKind === 'tfmx' && tfmxRendered >= TFMX_SECONDS * curSr) return { ended: true };
    const got = Module.ccall('rtx_stream_render', 'number', ['number','number'], [handle, frames]);
    if (got <= 0) return { ended: true };
    const { l, r } = readChunk(handle, got);
    tfmxRendered += got;
    return { ended: false, n: got, l, r, transfer: [l.buffer, r.buffer] };
  },

  async seek ({ t, sr }) {
    if (handle == null) throw new Error('kein offener Stream');
    Module.ccall('rtx_stream_seek', 'number', ['number','number'], [handle, t]);
    if (streamKind === 'tfmx') tfmxRendered = Math.floor(t * sr);
    return { baseFrames: Math.floor(t * sr) };
  },

  async destroy () {
    destroyHandle();
    return {};
  },

  // Kompletten Song vorab rendern (zu rechenintensiv fuer Echtzeit-Streaming,
  // oder Browser ganz ohne AudioWorklet). Laeuft in EINEM Rutsch durch - das
  // ist der eigentliche Sinn des Workers: der Haupt-Thread blockiert dabei
  // nicht, das alte setTimeout(0)-Yielding-Geruest entfaellt komplett.
  async renderFull ({ bufs }, onProgress) {
    await ensureModule();
    const sr = 44100;
    const h = Module.ccall('rtx_create', 'number', ['number'], [sr]);
    try {
      const meta = loadOnHandle(h, bufs);
      let estSec;
      if (meta.kind === 'tfmx') {
        const rc = Module.ccall('rtx_stream_start_tfmx', 'number', ['number','string'], [h, '/work/mdat.song']);
        if (rc < 0) throw new Error('TFMX nicht abspielbar (fehlt die zweite Datei mdat/smpl?).');
        estSec = TFMX_SECONDS;
      } else {
        Module.ccall('rtx_stream_start', 'number', ['number'], [h]);
        estSec = Math.max(1, Module.ccall('rtx_estimate_seconds', 'number', ['number'], [h]));
      }
      const chunks = [];
      let totalFrames = 0, safety = 0;
      while (true) {
        if (meta.kind === 'tfmx' && totalFrames >= TFMX_SECONDS * sr) break;
        const got = Module.ccall('rtx_stream_render', 'number', ['number','number'], [h, CHUNK_FRAMES]);
        if (got <= 0) break;
        chunks.push(readChunk(h, got));
        totalFrames += got;
        if (onProgress) onProgress(Math.min(0.99, totalFrames / sr / estSec));
        if (++safety > 20000) break;                 // Sicherheitsnetz gegen Endlosschleifen
      }
      const L = new Float32Array(totalFrames), R = new Float32Array(totalFrames);
      let off = 0;
      for (const c of chunks) { L.set(c.l, off); R.set(c.r, off); off += c.l.length; }
      if (onProgress) onProgress(1);
      return { label: meta.label, info: meta.info, sr, frames: totalFrames, l: L, r: R,
               transfer: [L.buffer, R.buffer] };
    } finally {
      Module.ccall('rtx_destroy', null, ['number'], [h]);
    }
  },
};

self.onmessage = async (e) => {
  const { id, cmd, args } = e.data;
  const fn = handlers[cmd];
  if (!fn) { self.postMessage({ id, type:'done', ok:false, error:'unbekanntes Kommando: ' + cmd }); return; }
  try {
    const onProgress = (p) => self.postMessage({ id, type:'progress', p });
    const result = await fn(args, onProgress);
    const transfer = result && result.transfer;
    if (transfer) delete result.transfer;
    self.postMessage({ id, type:'done', ok:true, result }, transfer || []);
  } catch (err) {
    self.postMessage({ id, type:'done', ok:false, error: String((err && err.message) || err) });
  }
};
