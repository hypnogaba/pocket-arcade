// host.ts: Pocket Arcade's badge simulator, made from the Solana OS C SDK's
// sdk/c/tests/mock-host.ts (MIT, Solana Foundation). The ABI checker is theirs;
// this copy adds what testing the app needs: the app's real bundled files
// (fs), a real card folder (files, saves), storage kept in a file across
// launches, a list with a cursor, ui_frame drawn into a screenshot, labels
// kept so a script can check the text, and a script of keys and ticks with
// relaunches (to test continuing a game after HOME).
//
//   node --experimental-strip-types tools/sim/host.ts build/badge/app.wasm
//     --app app --card build/card --storage build/storage.json
//     --script tools/sim/flow.json --shots build/shots [--quiet]
import { readFileSync, writeFileSync, existsSync, mkdirSync, readdirSync, statSync, unlinkSync } from 'node:fs';
import { createHash } from 'node:crypto';
import { deflateSync } from 'node:zlib';
import { join } from 'node:path';
const args = process.argv.slice(2);
const file = args.find((a, k) => !a.startsWith('--') && (k === 0 || !args[k - 1].startsWith('--')));
if (!file) {
  console.error('usage: host.ts app.wasm --app dir --card dir --storage file --script file --shots dir');
  process.exit(2);
}
const opt = (k: string, d: string) => {
  const i = args.indexOf(k);
  return i >= 0 ? args[i + 1] : d;
};
const APP = opt('--app', 'app');
const CARD = opt('--card', 'build/card');
const STORE = opt('--storage', 'build/storage.json');
const SCRIPT = opt('--script', '');
const SHOTS = opt('--shots', 'build/shots');
const quiet = args.includes('--quiet');
const trace = args.includes('--trace');

// --- values -------------------------------------------------------------------
const T = { NONE: -1, NIL: 0, BOOL: 1, INT: 2, NUM: 3, STR: 4, TABLE: 5, FN: 6, OBJ: 7 } as const;
type JsFn = (a: V[]) => V[];
type V =
  | { t: 0 }
  | { t: 1; v: boolean }
  | { t: 2; v: number }
  | { t: 3; v: number }
  | { t: 4; v: Uint8Array }
  | { t: 5; v: Map<string | number, V> }
  | { t: 6; js?: JsFn; c?: { idx: number; ud: number } }
  | { t: 7; v: { kind: string; methods: Record<string, JsFn> } };

const NIL: V = { t: 0 };
const enc = new TextEncoder();
const dec = new TextDecoder();
const b = (x: boolean): V => ({ t: 1, v: x });
const i = (x: number): V => ({ t: 2, v: x | 0 });
const s = (x: string | Uint8Array): V => ({ t: 4, v: typeof x === 'string' ? enc.encode(x) : x });
const str = (v: V) => (v.t === 4 ? dec.decode(v.v) : '');
function tbl(o: Record<string, unknown> | unknown[]): V {
  const m = new Map<string | number, V>();
  if (Array.isArray(o)) o.forEach((x, k) => m.set(k + 1, js2v(x)));
  else for (const [k, x] of Object.entries(o)) if (x !== undefined) m.set(k, js2v(x));
  return { t: 5, v: m };
}
function js2v(x: unknown): V {
  if (x === null || x === undefined) return NIL;
  if (typeof x === 'boolean') return b(x);
  if (typeof x === 'number') return Number.isInteger(x) ? i(x) : { t: 3, v: Math.fround(x) };
  if (typeof x === 'string') return s(x);
  if (x instanceof Uint8Array) return s(x);
  if (typeof x === 'object' && 't' in (x as object)) return x as V;
  return tbl(x as Record<string, unknown>);
}
function v2js(v: V): unknown {
  switch (v.t) {
    case 0: return null;
    case 1: case 2: case 3: return v.v;
    case 4: return dec.decode(v.v);
    case 5: {
      const m = v.v;
      const n = tlen(v);
      if (n > 0 && n === m.size) return Array.from({ length: n }, (_, k) => v2js(m.get(k + 1)!));
      return Object.fromEntries([...m].map(([k, x]) => [String(k), v2js(x)]));
    }
    default: return `<${v.t === 6 ? 'function' : 'object'}>`;
  }
}
function tlen(v: V) {
  if (v.t !== 5) return 0;
  let n = 0;
  while (v.v.has(n + 1) && v.v.get(n + 1)!.t !== 0) n++;
  return n;
}
const fn = (js: JsFn): V => ({ t: 6, js });

// --- problems -------------------------------------------------------------------
const problems: string[] = [];
let calls = 0;
const callsByName = new Map<string, number>();
class AppBug extends Error {}
const bug = (m: string): never => {
  throw new AppBug(m);
};

// --- the fake Lua API -------------------------------------------------------------
// want(args, spec, name): i int, n number, s string, t table, f function, b bool,
// o object, x string-or-table-or-object (an image source), c colour (string or
// int), * anything; a trailing ? makes it optional (nil or missing).
function want(a: V[], spec: string, name: string) {
  const parts = spec.split(' ').filter(Boolean);
  if (a.length > parts.length) bug(`${name}: ${a.length} arguments, expected at most ${parts.length} (${spec})`);
  parts.forEach((p, k) => {
    const optional = p.endsWith('?');
    const ty = p.replace('?', '');
    const v = a[k] ?? NIL;
    if (v.t === 0 && optional) return;
    const ok =
      ty === '*' ||
      (ty === 'i' && v.t === 2) ||
      (ty === 'n' && (v.t === 2 || v.t === 3)) ||
      (ty === 's' && v.t === 4) ||
      (ty === 't' && v.t === 5) ||
      (ty === 'f' && v.t === 6) ||
      (ty === 'b' && v.t === 1) ||
      (ty === 'o' && v.t === 7) ||
      (ty === 'c' && (v.t === 4 || v.t === 2)) ||
      (ty === 'x' && (v.t === 4 || v.t === 5 || v.t === 7)) ||
      (ty === 'tb' && (v.t === 5 || v.t === 1));
    if (!ok) bug(`${name}: argument ${k + 1} is type ${v.t}, expected ${ty} (${spec})`);
    if (ty === 'c' && v.t === 4 && !THEME.includes(str(v))) bug(`${name}: unknown colour "${str(v)}"`);
  });
}
const THEME = ['text', 'dim', 'bg', 'panel', 'border', 'purple', 'green', 'pink', 'lpurple', 'blue', 'aqua', 'lime', 'yellow', 'orange'];

type Pending = { f: V; args: V[]; what: string };
const queue: Pending[] = [];
const later = (f: V, args: unknown[], what: string) => queue.push({ f, args: args.map(js2v), what });

function obj(kind: string, extra: Record<string, JsFn> = {}): V {
  const m: Record<string, JsFn> = {
    pos: (a) => (want(a, 'o i i', `${kind}:pos`), []),
    show: (a) => (want(a, 'o b', `${kind}:show`), []),
    remove: (a) => (want(a, 'o', `${kind}:remove`), []),
    ...extra,
  };
  return { t: 7, v: { kind, methods: m } };
}
const uiObj = (kind: string) =>
  obj(kind, {
    text: (a) => (want(a, 'o s', `${kind}:text`), []),
    color: (a) => (want(a, 'o c', `${kind}:color`), []),
    size: (a) => (want(a, 'o i i', `${kind}:size`), []),
    set: (a) => (want(a, 'o *', `${kind}:set`), []),
    scale: (a) => (want(a, 'o i', `${kind}:scale`), []),
  });
const store = new Map<string, V>();
let timerId = 0;

const api: Record<string, V> = {
  'ui.W': i(320),
  'ui.H': i(218),
  'ui.bg': fn((a) => (want(a, 'c', 'ui.bg'), [])),
  'ui.label': fn((a) => (want(a, 'i i s t?', 'ui.label'), [uiObj('label')])),
  'ui.rect': fn((a) => (want(a, 'i i i i t?', 'ui.rect'), [uiObj('rect')])),
  'ui.bar': fn((a) => (want(a, 'i i i i i', 'ui.bar'), [obj('bar', { set: (x) => (want(x, 'o i', 'bar:set'), []) })])),
  'ui.gradient': fn((a) => (want(a, 'i i i i', 'ui.gradient'), [obj('gradient')])),
  'ui.list': fn((a) => {
    want(a, 'i i i i t t?', 'ui.list');
    const o = a[5];
    const cb = o?.t === 5 ? o.v.get('on_select') : undefined;
    if (cb && cb.t !== 6) bug('ui.list: on_select is not a function');
    if (cb) later(cb, [1, a[4].t === 5 ? a[4].v.get(1) ?? NIL : NIL], 'list on_select');
    return [obj('list', {
      items: (x) => (want(x, 'o t', 'list:items'), []),
      selected: (x) => (want(x, 'o', 'list:selected'), [i(1)]),
      select: (x) => (want(x, 'o i', 'list:select'), []),
    })];
  }),
  'ui.qr': fn((a) => (want(a, 'i i i s', 'ui.qr'), [uiObj('qr')])),
  'ui.bg': fn((a) => (want(a, 'c', 'ui.bg'), screen.fill((a[0] as any).v === 0x01000000 ? 0 : 40), [])),
  'ui.hint': fn((a) => (want(a, 's', 'ui.hint'), [])),
  'ui.toast': fn((a) => (want(a, 's', 'ui.toast'), [])),
  'ui.confirm': fn((a) => (want(a, 's f', 'ui.confirm'), later(a[1], [true], 'confirm'), [])),
  'ui.input': fn((a) => (want(a, 's i f', 'ui.input'), later(a[2], ['typed'], 'input'), [])),
  'ui.clear': fn((a) => (want(a, '', 'ui.clear'), [])),
  'ui.load': fn((a) => (want(a, 'x t?', 'ui.load'), [obj('img', { size: (x) => (want(x, 'o', 'img:size'), [i(16), i(8)]) })])),
  'ui.image': fn((a) => (want(a, 'i i x t?', 'ui.image'), [uiObj('image')])),
  'ui.canvas': fn((a) => {
    want(a, 'i i i i t?', 'ui.canvas');
    return [obj('canvas', {
      clear: (x) => (want(x, 'o c?', 'canvas:clear'), []),
      pixel: (x) => (want(x, 'o i i c', 'canvas:pixel'), []),
      rect: (x) => (want(x, 'o i i i i c', 'canvas:rect'), []),
      line: (x) => (want(x, 'o i i i i c', 'canvas:line'), []),
      image: (x) => (want(x, 'o x i i', 'canvas:image'), []),
      get: (x) => (want(x, 'o i i', 'canvas:get'), [i(0x9c45ff)]),
    })];
  }),
  'ui.anim': fn((a) => {
    want(a, 'i i s t?', 'ui.anim');
    return [obj('anim', {
      play: (x) => (want(x, 'o', 'anim:play'), []),
      stop: (x) => (want(x, 'o', 'anim:stop'), []),
      playing: (x) => (want(x, 'o', 'anim:playing'), [b(true)]),
      scale: (x) => (want(x, 'o i', 'anim:scale'), []),
    })];
  }),
  'led.set': fn((a) => (want(a, 'i i i i', 'led.set'), (a[0].t === 2 && (a[0].v < 1 || a[0].v > 8) ? bug(`led.set: LED ${a[0].v}`) : 0), [])),
  'led.hex': fn((a) => (want(a, 'i i', 'led.hex'), [])),
  'led.fill': fn((a) => (want(a, 'i i i', 'led.fill'), [])),
  'led.off': fn((a) => (want(a, '', 'led.off'), [])),
  'timer.after': fn((a) => (want(a, 'i f', 'timer.after'), later(a[1], [], 'timer.after'), [i(++timerId)])),
  'timer.every': fn((a) => (want(a, 'i f', 'timer.every'), later(a[1], [], 'timer.every'), later(a[1], [], 'timer.every'), [i(++timerId)])),
  'timer.cancel': fn((a) => (want(a, 'i', 'timer.cancel'), [])),
  'sys.millis': fn(() => [i(1234)]),
  'sys.time': fn(() => [i(1790000000)]),
  'sys.battery': fn(() => [NIL]),
  'sys.keys': fn(() => [tbl({ up: true })]),
  'sys.memory': fn(() => [tbl({ used: 18773, max: 6291456 })]),
  'sys.exit': fn(() => []),
  'badge.id': fn(() => [s('7K2Q9M')]),
  'badge.owner': fn(() => [NIL]),
  'badge.online': fn(() => [b(true)]),
  'json.encode': fn((a) => (want(a, '*', 'json.encode'), [s(JSON.stringify(v2js(a[0] ?? NIL)))])),
  'json.decode': fn((a) => {
    want(a, 's', 'json.decode');
    try {
      return [js2v(JSON.parse(str(a[0])))];
    } catch {
      return [NIL, s('bad JSON')];
    }
  }),
  'storage.get': fn((a) => (want(a, 's', 'storage.get'), [store.get(str(a[0])) ?? NIL])),
  'storage.set': fn((a) => (want(a, 's *', 'storage.set'), store.set(str(a[0]), a[1]), [])),
  'storage.remove': fn((a) => (want(a, 's', 'storage.remove'), store.delete(str(a[0])), [])),
  'sys.tick': fn((a) => (want(a, 'i', 'sys.tick'), [i(30)])),
  'files.dir': fn((a) => (want(a, '', 'files.dir'), [s('hh/files/app/')])),
  'files.size': fn((a) => (want(a, 's', 'files.size'), [i(32768)])),
  'files.list': fn((a) => (want(a, '', 'files.list'), [tbl([tbl({ name: 'game.gb', size: 32768 }), tbl({ name: 'save/game.sav', size: 8192 })])])),
  'files.write': fn((a) => (want(a, 's i s', 'files.write'), [b(true)])),
  'files.remove': fn((a) => (want(a, 's', 'files.remove'), [b(true)])),
  'fs.exists': fn((a) => (want(a, 's', 'fs.exists'), [b(true)])),
  'fs.size': fn((a) => (want(a, 's', 'fs.size'), [i(9)])),
  'fs.read': fn((a) => (want(a, 's i? i?', 'fs.read'), str(a[0]) === 'missing' ? [NIL, s('not found')] : [s('file data')])),
  'fs.list': fn((a) => (want(a, 's?', 'fs.list'), [tbl(['a.txt', 'dir/', 'z.png'])])),
  'fs.lines': fn((a) => {
    want(a, 's', 'fs.lines');
    let n = 0;
    return [fn(() => (n < 3 ? [s(`line ${++n}`)] : [NIL]))];
  }),
  'fs.json': fn((a) => (want(a, 's', 'fs.json'), [tbl({ level: 1, name: 'one' })])),
  'wifi.status': fn(() => [s('on')]),
  'wifi.ssid': fn(() => [s('Net')]),
  'wifi.on': fn(() => []),
  'wifi.off': fn(() => []),
  'mesh.nearby': fn(() => [tbl([
    { id: '7K2Q9M', name: '@toly', level: 12, rssi: -48, kind: 'badge', bridged: false },
    { id: 'BEAC01', name: 'Marvin', level: 0, rssi: -70, kind: 'beacon', bridged: false },
  ])]),
  'mesh.send': fn((a) => (want(a, 's t', 'mesh.send'), [b(true)])),
  'mesh.bridge': fn(() => []),
  'mesh.pause': fn(() => []),
  'mesh.resume': fn(() => []),
  'mesh.info': fn(() => [tbl({ channel: 6, band: '2.4', online: true, paused: false, event: 6, scanning: false })]),
  'mesh.channel': fn((a) => (want(a, 'i', 'mesh.channel'), a[0].t === 2 && a[0].v === 99 ? [NIL, s('not a mesh channel')] : [b(true)])),
  'mesh.scan': fn((a) => {
    want(a, 't f', 'mesh.scan');
    later(a[1], [[
      { channel: 1, band: '2.4', badges: 0, beacons: 0, app: 0, peers: [] },
      { channel: 6, band: '2.4', badges: 1, beacons: 0, app: 1, peers: [{ id: '7K2Q9M', kind: 'badge', name: '@toly', level: 12, rssi: -48, app: true }] },
    ]], 'mesh.scan');
    return [b(true)];
  }),
  'mesh.meet': fn((a) => (want(a, 's t f', 'mesh.meet'), later(a[2], [true, { peer: str(a[0]), data: { caught: 1 } }], 'mesh.meet'), later(a[2], [false, 'refused'], 'mesh.meet (2nd, stale)'), [b(true)])),
  'mesh.meets': fn((a) => {
    want(a, a.length === 2 ? 'i f' : 'f', 'mesh.meets');
    later(a[a.length - 1], [true, { items: [{ id: 1, peer: '7K2Q9M', asked: true, mine: {}, theirs: {}, bridged: false, at: 1790000000 }] }], 'mesh.meets');
    return [];
  }),
  'ble.on': fn(() => [b(true)]),
  'ble.off': fn(() => []),
  'ble.state': fn(() => [s('on')]),
  'ble.address': fn(() => [s('7c:df:a1:00:56:78')]),
  'ble.advertise': fn((a) => (want(a, 'tb', 'ble.advertise'), [b(true)])),
  'ble.scan': fn((a) => {
    want(a, 'tb f?', 'ble.scan');
    if (a[1]) later(a[1], [{ addr: '7c:df:a1:00:12:34', rssi: -61, name: 'Dice', services: ['ffe0'], company: 0xffff, data: '\x01\x02', connectable: true, random: false }], 'ble.scan');
    return [b(true)];
  }),
  'ble.connect': fn((a) => {
    want(a, 't f', 'ble.connect');
    if (str(a[0].t === 5 ? a[0].v.get('addr') ?? NIL : NIL) === '') bug('ble.connect: no addr');
    later(a[1], ['open'], 'link open');
    later(a[1], ['notify', 'ffe1', '\x01\x05'], 'link notify');
    later(a[1], ['close', 'lost'], 'link close');
    later(a[1], ['notify', 'ffe1', 'late'], 'link event after close (stale)');
    return [obj('link', {
      read: (x) => (want(x, 'o s s f', 'link:read'), later(x[3], [true, '\x01\x02'], 'link:read'), [b(true)]),
      write: (x) => (want(x, 'o s s s f?', 'link:write'), x[4] ? later(x[4], [false, 'timeout'], 'link:write') : 0, [b(true)]),
      subscribe: (x) => (want(x, 'o s s b?', 'link:subscribe'), [b(true)]),
      close: (x) => (want(x, 'o', 'link:close'), []),
      state: (x) => (want(x, 'o', 'link:state'), [s('open')]),
    })];
  }),
  'ble.serve': fn((a) => {
    want(a, 't f', 'ble.serve');
    const chars = a[0].t === 5 ? a[0].v.get('chars') : undefined;
    if (!chars || tlen(chars) < 1 || tlen(chars) > 4) bug('ble.serve: 1 to 4 chars');
    later(a[1], ['open', 'aa:bb:cc:dd:ee:ff'], 'serve open');
    later(a[1], ['write', 'ffe1', 'hello'], 'serve write');
    later(a[1], ['subscribe', 'ffe1', true], 'serve subscribe');
    later(a[1], ['close', 'peer closed'], 'serve close');
    return [b(true)];
  }),
  'ble.set': fn((a) => (want(a, 's s', 'ble.set'), [b(true)])),
  'ble.notify': fn((a) => (want(a, 's s', 'ble.notify'), [NIL, s('not subscribed')])),
  'ble.drop': fn(() => []),
  'lamps.balance': fn((a) => (want(a, 'f', 'lamps.balance'), later(a[0], [true, 1250], 'lamps.balance'), [])),
  'lamps.spend': fn((a) => (want(a, 'i s f', 'lamps.spend'), later(a[2], [true, { balance: 1200 }], 'lamps.spend'), [])),
  'lamps.transfer': fn((a) => (want(a, 's i s f', 'lamps.transfer'), later(a[3], [false, 'denied'], 'lamps.transfer'), [])),
  'lamps.deposit': fn((a) => (want(a, 's i s f', 'lamps.deposit'), later(a[3], [true, { balance: 1150, id: '4821' }], 'lamps.deposit'), [])),
  'lamps.award': fn((a) => (want(a, 'i s f', 'lamps.award'), later(a[2], [true, { awarded: 0, balance: 1250, needsX: true }], 'lamps.award'), [])),
  'items.list': fn((a) => (want(a, 'f', 'items.list'), later(a[0], [true, [{ id: 'it1', kind: 'virus', name: 'Omega', data: { x: 1 }, tradable: true }]], 'items.list'), [])),
  'items.all': fn((a) => (want(a, 'f', 'items.all'), later(a[0], [true, []], 'items.all'), [])),
  'items.grant': fn((a) => (want(a, 's s t f', 'items.grant'), later(a[3], [true, { id: 'it2', kind: str(a[0]), name: str(a[1]), data: v2js(a[2]) }], 'items.grant'), [])),
  'items.update': fn((a) => (want(a, 's t f', 'items.update'), later(a[2], [true, { id: str(a[0]) }], 'items.update'), [])),
  'items.remove': fn((a) => (want(a, 's f', 'items.remove'), later(a[1], [true, { ok: true }], 'items.remove'), [])),
  'profile.me': fn((a) => (want(a, 'f', 'profile.me'), later(a[0], [true, { name: 'Toly', x: 'toly', xName: 'toly', level: 12, lamps: 1250, pages: 30, badge: '7K2Q9M' }], 'profile.me'), [])),
  'profile.get': fn((a) => (want(a, 's f', 'profile.get'), later(a[1], [true, { name: 'Ann', level: 3 }], 'profile.get'), [])),
  // A first page with a cursor, then the last page (the cursor is the server's opaque text).
  'profile.items': fn((a) => {
    want(a, a.length === 2 ? 's f' : 'f', 'profile.items');
    const last = a.length === 2;
    if (last && str(a[0]) !== 'WzE3OTAwMDAwMDAwMDAsIml0XzAxIl0') bug('profile.items: not the cursor it was given');
    later(a[a.length - 1], [true, {
      items: last ? [{ id: 'it_02', kind: 'towel', name: 'Towel', quest: 'towel-run', tradable: false }]
        : [{ id: 'it_01', kind: 'gem', name: 'Gem', app: 'gem-forge', tradable: true }],
      next: last ? undefined : 'WzE3OTAwMDAwMDAwMDAsIml0XzAxIl0', total: 2,
    }], 'profile.items');
    return [];
  }),
  'profile.pages': fn((a) => {
    want(a, a.length === 2 ? 's f' : 'f', 'profile.pages');
    later(a[a.length - 1], [true, { pages: [{ id: 'p-01', number: 1, title: 'Genesis', category: 'history', rarity: 'common', count: 2 }], owned: 1, total: 64 }], 'profile.pages');
    return [];
  }),
  'quest.report': fn((a) => (want(a, 's t f?', 'quest.report'), a[2] ? later(a[2], [true, {}], 'quest.report') : 0, [])),
  'quest.active': fn((a) => (want(a, 'f', 'quest.active'), later(a[0], [true, [{ id: 'q1', title: 'Virus Lab', step: 'Mutate it' }]], 'quest.active'), [])),
  'http.get': fn((a) => {
    want(a, a.length === 2 ? 's f' : 's t f', 'http.get');
    const cb = a[a.length - 1];
    if (cb.t !== 6) bug('http.get: no callback');
    if (!str(a[0]).startsWith('https://')) throw 'http.get: https:// only';
    later(cb, [true, { status: 200, body: '{"data":{"amount":"187.12","base":"SOL","currency":"USD"}}', type: 'application/json', truncated: false }], 'http.get');
    return [];
  }),
  'http.post': fn((a) => {
    want(a, a.length === 3 ? 's * f' : 's * t f', 'http.post');
    later(a[a.length - 1], [false, 'offline'], 'http.post');
    return [];
  }),
  'http.request': fn((a) => (want(a, 't f', 'http.request'), later(a[1], [true, { status: 301, body: '', type: 'text/html', truncated: false, location: 'https://x/' }], 'http.request'), [])),
  'ws.open': fn((a) => {
    want(a, a.length === 2 ? 's f' : 's t f', 'ws.open');
    const cb = a[a.length - 1];
    later(cb, ['open'], 'ws open');
    later(cb, ['message', '{"from":"ann","text":"hi"}'], 'ws message');
    later(cb, ['close', 'host closed', 1000], 'ws close');
    later(cb, ['message', 'late'], 'ws message after close (stale)');
    return [obj('socket', {
      send: (x) => (want(x, 'o *', 'socket:send'), [b(true)]),
      close: (x) => (want(x, 'o', 'socket:close'), []),
      state: (x) => (want(x, 'o', 'socket:state'), [s('connecting')]),
    })];
  }),
  'rpc.call': fn((a) => {
    want(a, a.length === 3 ? 's t? f' : 's t? t f', 'rpc.call');
    later(a[a.length - 1], [true, { context: { slot: 371234567 }, value: '5000000000' }], 'rpc.call');
    return [];
  }),
  'crypto.sha256': fn((a) => (want(a, 's', 'crypto.sha256'), [s(new Uint8Array(createHash('sha256').update(a[0].t === 4 ? a[0].v : new Uint8Array()).digest()))])),
  'crypto.on_curve': fn((a) => (want(a, 's', 'crypto.on_curve'), [b(true)])),
  'crypto.verify': fn((a) => (want(a, 's s s', 'crypto.verify'), [b(false)])),
  'solana.encode': fn((a) => (want(a, 's', 'solana.encode'), [s(b58(a[0].t === 4 ? a[0].v : new Uint8Array()))])),
  'solana.decode': fn((a) => {
    want(a, 's', 'solana.decode');
    const r = unb58(str(a[0]));
    return r ? [s(r)] : [NIL, s('not base58')];
  }),
  'solana.pda': fn((a) => (want(a, 't s', 'solana.pda'), [s(new Uint8Array(32).fill(7)), i(254)])),
};

// --- Pocket Arcade: the parts of the API the app uses, backed by real files ---
type Label = { x: number; y: number; text: string; shown: boolean };
type ListState = { items: V; cursor: number; cb?: V; alive: boolean };
const labels = new Map<number, Label>();
let objSeq = 0;
let focusList: ListState | null = null;
const toasts: string[] = [];
let hint = '';
const leds: number[][] = Array.from({ length: 8 }, () => [0, 0, 0]);
const KEYNAMES = ['up', 'down', 'left', 'right', 'select', 'back'];
const persisted: Record<string, unknown> = existsSync(STORE) ? JSON.parse(readFileSync(STORE, 'utf8')) : {};
for (const [k, v] of Object.entries(persisted)) store.set(k, js2v(v));
const saveStore = () => {
  const o: Record<string, unknown> = {};
  for (const [k, v] of store) o[k] = v2js(v);
  writeFileSync(STORE, JSON.stringify(o, null, 1));
};
mkdirSync(join(CARD, 'save'), { recursive: true });
const fileName = (n: string) => {
  if (!/^(save\/)?[ -~]{1,64}$/.test(n) || /[\\:*?"<>|]/.test(n.replace(/^save\//, '')) || n.replace(/^save\//, '').includes('/')) bug(`files: bad name "${n}"`);
  return join(CARD, n);
};
function listItemText(v: V): string {
  if (v.t === 4) return str(v);
  if (v.t === 5) return ['title', 'sub', 'right'].map((k) => str(v.v.get(k) ?? NIL)).filter(Boolean).join(' | ');
  return '';
}

Object.assign(api, {
  'ui.label': fn((a) => {
    want(a, 'i i s t?', 'ui.label');
    const id = ++objSeq;
    labels.set(id, { x: (a[0] as any).v, y: (a[1] as any).v, text: str(a[2]), shown: true });
    if (str(a[2]).length > 200) bug('ui.label: more than 200 bytes');
    return [obj('label', {
      text: (x) => (want(x, 'o s', 'label:text'), labels.has(id) && (labels.get(id)!.text = str(x[1])), []),
      color: (x) => (want(x, 'o c', 'label:color'), []),
      show: (x) => (want(x, 'o b', 'label:show'), labels.has(id) && (labels.get(id)!.shown = (x[1] as any).v), []),
      remove: (x) => (want(x, 'o', 'label:remove'), labels.delete(id), []),
    })];
  }),
  'ui.list': fn((a) => {
    want(a, 'i i i i t t?', 'ui.list');
    const o = a[5];
    const cb = o?.t === 5 ? o.v.get('on_select') : undefined;
    if (cb && cb.t !== 6) bug('ui.list: on_select is not a function');
    const L: ListState = { items: a[4], cursor: 0, cb, alive: true };
    focusList = L;
    return [obj('list', {
      items: (x) => (want(x, 'o t', 'list:items'), (L.items = x[1]), (L.cursor = Math.min(L.cursor, Math.max(0, tlen(x[1]) - 1))), []),
      selected: (x) => (want(x, 'o', 'list:selected'), [i(tlen(L.items) ? L.cursor + 1 : 0)]),
      select: (x) => {
        want(x, 'o i', 'list:select');
        const k = (x[1] as any).v;
        if (k < 1 || k > tlen(L.items)) bug(`list:select(${k}) with ${tlen(L.items)} rows`);
        L.cursor = k - 1;
        return [];
      },
      remove: (x) => (want(x, 'o', 'list:remove'), (L.alive = false), focusList === L && (focusList = null), []),
      pos: (x) => (want(x, 'o i i', 'list:pos'), []),
      show: (x) => (want(x, 'o b', 'list:show'), []),
    })];
  }),
  'ui.clear': fn((a) => (want(a, '', 'ui.clear'), screen.fill(40), labels.clear(), focusList && (focusList.alive = false), (focusList = null), (hint = ''), [])),
  'ui.hint': fn((a) => (want(a, 's', 'ui.hint'), (hint = str(a[0])), [])),
  'ui.toast': fn((a) => (want(a, 's', 'ui.toast'), toasts.push(str(a[0])), out(`[toast] ${str(a[0])}`), [])),
  'sys.keys': fn(() => {
    const t: Record<string, boolean> = {};
    KEYNAMES.forEach((n, k) => heldKeys & (1 << k) && (t[n] = true));
    return [tbl(t)];
  }),
  'sys.time': fn(() => [i(1790000000)]),
  'storage.set': fn((a) => (want(a, 's *', 'storage.set'), store.set(str(a[0]), a[1]), saveStore(), [])),
  'storage.remove': fn((a) => (want(a, 's', 'storage.remove'), store.delete(str(a[0])), saveStore(), [])),
  'files.dir': fn((a) => (want(a, '', 'files.dir'), [s('hh/files/pocket-arcade/')])),
  'files.size': fn((a) => {
    want(a, 's', 'files.size');
    const p = fileName(str(a[0]));
    return existsSync(p) ? [i(statSync(p).size)] : [NIL];
  }),
  'files.list': fn((a) => {
    want(a, '', 'files.list');
    const own = readdirSync(CARD).filter((n) => statSync(join(CARD, n)).isFile() && !n.startsWith('.'));
    const saves = readdirSync(join(CARD, 'save')).map((n) => 'save/' + n);
    return [tbl([...own, ...saves].slice(0, 128).map((n) => ({ name: n, size: statSync(join(CARD, n)).size })))];
  }),
  'files.write': fn((a) => {
    want(a, 's i s', 'files.write');
    const n = str(a[0]);
    if (!n.startsWith('save/')) return [NIL, s('only save/<name> can be written')];
    const p = fileName(n);
    const off = (a[1] as any).v as number;
    const data = (a[2] as any).v as Uint8Array;
    if (data.length > 256 * 1024) return [NIL, s('256 KB a call')];
    const cur = existsSync(p) ? readFileSync(p) : Buffer.alloc(0);
    if (off > cur.length) return [NIL, s('no holes')];
    if (!existsSync(p) && readdirSync(join(CARD, 'save')).length >= 32) return [NIL, s('at most 32 files in save/')];
    const next = Buffer.concat([cur.subarray(0, off), Buffer.from(data), cur.subarray(Math.min(cur.length, off + data.length))]);
    if (next.length > 1024 * 1024) return [NIL, s('a save file is at most 1 MB')];
    writeFileSync(p, next);
    return [b(true)];
  }),
  'files.remove': fn((a) => {
    want(a, 's', 'files.remove');
    const n = str(a[0]);
    if (!n.startsWith('save/')) return [NIL, s('only saves')];
    const p = fileName(n);
    if (!existsSync(p)) return [b(false)];
    unlinkSync(p);
    return [b(true)];
  }),
  'fs.exists': fn((a) => (want(a, 's', 'fs.exists'), [b(existsSync(join(APP, str(a[0]))))])),
  'fs.size': fn((a) => {
    want(a, 's', 'fs.size');
    const p = join(APP, str(a[0]));
    return existsSync(p) ? [i(statSync(p).size)] : [NIL];
  }),
  'fs.read': fn((a) => {
    want(a, 's i? i?', 'fs.read');
    const name = str(a[0]);
    if (/\.(c|h)$/.test(name)) return [NIL, s('not found')];   // the badge never gets the source
    const p = join(APP, name);
    if (!existsSync(p)) return [NIL, s('not found')];
    const all = readFileSync(p);
    if (all.length > 256 * 1024) bug(`fs.read: ${name} is over 256 KB`);
    if (a[1] && a[1].t === 2) {
      const off = (a[1] as any).v, len = a[2] && a[2].t === 2 ? (a[2] as any).v : all.length;
      if (off < 0 || len < 0) bug('fs.read: negative offset or length');
      return [s(new Uint8Array(all.subarray(off, off + len)))];
    }
    return [s(new Uint8Array(all))];
  }),
  'led.set': fn((a) => {
    want(a, 'i i i i', 'led.set');
    const k = (a[0] as any).v;
    if (k < 1 || k > 8) bug(`led.set: LED ${k} (LEDs are 1..8)`);
    leds[k - 1] = [(a[1] as any).v, (a[2] as any).v, (a[3] as any).v];
    return [];
  }),
  'led.off': fn((a) => (want(a, '', 'led.off'), leds.forEach((l) => l.fill(0)), [])),
  'led.fill': fn((a) => (want(a, 'i i i', 'led.fill'), leds.forEach((l, k) => (leds[k] = [(a[0] as any).v, (a[1] as any).v, (a[2] as any).v])), [])),
});

const B58 = '123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz';
function b58(bytes: Uint8Array) {
  let n = 0n;
  for (const x of bytes) n = n * 256n + BigInt(x);
  let out = '';
  while (n > 0n) { out = B58[Number(n % 58n)] + out; n /= 58n; }
  for (const x of bytes) { if (x) break; out = '1' + out; }
  return out;
}
function unb58(t: string) {
  let n = 0n;
  for (const c of t) { const d = B58.indexOf(c); if (d < 0) return null; n = n * 58n + BigInt(d); }
  const out: number[] = [];
  while (n > 0n) { out.unshift(Number(n % 256n)); n /= 256n; }
  for (const c of t) { if (c !== '1') break; out.unshift(0); }
  return new Uint8Array(out);
}

// --- the ABI ----------------------------------------------------------------------
const frames: V[][] = [];
const F = () => frames[frames.length - 1] ?? bug('no frame (an import called outside a callback)');
const MAXF = 64;
function abs(idx: number) {
  const f = F();
  const a = idx > 0 ? idx : f.length + idx + 1;
  if (idx === 0 || a < 1 || a > f.length) bug(`index ${idx} outside the frame (${f.length} values)`);
  return a;
}
const at = (idx: number) => F()[abs(idx) - 1];
function push(v: V) {
  const f = F();
  if (f.length >= MAXF) bug('more than 64 values in a frame');
  f.push(v);
}
const refs = new Map<number, V>();
let nextRef = 1;
let mem: WebAssembly.Memory;
let table: WebAssembly.Table;
const u8 = () => new Uint8Array(mem.buffer);
function bytes(p: number, n: number) {
  if (p < 0 || n < 0 || p + n > mem.buffer.byteLength) bug(`pointer ${p}+${n} outside the app's memory`);
  return u8().slice(p, p + n);
}
const cstr = (p: number, n: number) => dec.decode(bytes(p >>> 0, n >>> 0));
function lookup(path: string): V {
  if (path in api) return api[path];
  return NIL;
}
const unknownNames = new Set<string>();
for (const [k, v] of Object.entries(api)) if (v.t === 6 && v.js) Object.defineProperty(v.js, 'name', { value: k });

function callC(f: V & { t: 6 }, a: V[], nres: number, what: string): V[] {
  const fnc = table.get(f.c!.idx) as ((ud: number, n: number) => number) | null;
  if (!fnc) bug(`${what}: function table entry ${f.c!.idx} is empty`);
  frames.push([...a]);
  let r: number;
  try {
    r = fnc!(f.c!.ud, a.length);
  } finally {
    /* frame popped below */
  }
  const fr = frames.pop()!;
  if (r < 0 || r > fr.length) bug(`${what}: returned ${r} results with ${fr.length} values on its frame`);
  if (fr.length !== a.length + r) problems.push(`${what}: left ${fr.length} values on its frame (${a.length} arguments + ${r} results expected)`);
  const res = fr.slice(fr.length - r);
  if (nres === -1) return res;
  while (res.length < nres) res.push(NIL);
  return res.slice(0, nres);
}

const badgeImports = {
  top: () => F().length,
  pop: (n: number) => {
    const f = F();
    if (n < 0 || n > f.length) bug(`sos_pop(${n}) with ${f.length} values`);
    f.length -= n;
  },
  push_value: (idx: number) => push(at(idx)),
  remove: (idx: number) => void F().splice(abs(idx) - 1, 1),
  push_nil: () => push(NIL),
  push_bool: (x: number) => push(b(!!x)),
  push_int: (x: number) => push(i(x)),
  push_num: (x: number) => push({ t: 3, v: x }),
  push_str: (p: number, n: number) => push(s(bytes(p >>> 0, n >>> 0))),
  new_table: () => push({ t: 5, v: new Map() }),
  push_fn: (idx: number, ud: number) => {
    if (!table.get(idx)) bug(`sos_push_fn: ${idx} is not a function`);
    push({ t: 6, c: { idx, ud } });
  },
  type: (idx: number) => {
    const f = F();
    const a = idx > 0 ? idx : f.length + idx + 1;
    return a < 1 || a > f.length ? T.NONE : f[a - 1].t;
  },
  to_bool: (idx: number) => {
    const v = at(idx);
    return v.t === 0 || (v.t === 1 && !v.v) ? 0 : 1;
  },
  to_int: (idx: number) => {
    const v = at(idx);
    return v.t === 2 || v.t === 3 ? Math.trunc(v.v) | 0 : 0;
  },
  to_num: (idx: number) => {
    const v = at(idx);
    return v.t === 2 || v.t === 3 ? v.v : 0;
  },
  to_str: (idx: number, p: number, cap: number) => {
    const v = at(idx);
    cap >>>= 0;
    if (v.t !== 4) {
      if (cap) bytes(p >>> 0, 1), (u8()[p >>> 0] = 0);
      return 0;
    }
    if (cap) {
      const n = Math.min(v.v.length, cap - 1);
      bytes(p >>> 0, n + 1);
      u8().set(v.v.subarray(0, n), p >>> 0);
      u8()[(p >>> 0) + n] = 0;
    }
    return v.v.length;
  },
  len: (idx: number) => {
    const v = at(idx);
    return v.t === 4 ? v.v.length : v.t === 5 ? tlen(v) : 0;
  },
  get: (idx: number, k: number, kl: number) => {
    const v = at(idx);
    const key = cstr(k, kl);
    let r: V = NIL;
    if (v.t === 5) r = v.v.get(key) ?? NIL;
    else if (v.t === 7) r = v.v.methods[key] ? fn(v.v.methods[key]) : NIL;
    push(r);
    return r.t;
  },
  set: (idx: number, k: number, kl: number) => {
    const t = at(idx);
    const key = cstr(k, kl);
    const f = F();
    if (!f.length) bug('sos_set with an empty frame');
    const v = f.pop()!;
    if (t.t !== 5) bug(`sos_set on a value of type ${t.t}`);
    if (v.t === 0) t.v.delete(key);
    else t.v.set(key, v);
  },
  geti: (idx: number, n: number) => {
    const v = at(idx);
    const r = v.t === 5 ? v.v.get(n) ?? NIL : NIL;
    push(r);
    return r.t;
  },
  seti: (idx: number, n: number) => {
    const t = at(idx);
    const f = F();
    const v = f.pop()!;
    if (t.t !== 5) bug(`sos_seti on a value of type ${t.t}`);
    t.v.set(n, v);
  },
  next: (idx: number) => {
    const t = at(idx);
    const k = F().pop()!;
    if (t.t !== 5) bug('sos_next on a non-table');
    const keys = [...t.v.keys()];
    const kk = k.t === 0 ? -1 : keys.findIndex((x) => (typeof x === 'number' ? k.t === 2 && k.v === x : k.t === 4 && str(k) === x));
    const nk = keys[kk + 1];
    if (nk === undefined) return 0;
    push(typeof nk === 'number' ? i(nk) : s(nk));
    push(t.v.get(nk)!);
    return 1;
  },
  global: (p: number, n: number) => {
    const path = cstr(p, n);
    const v = lookup(path);
    if (v.t === 0) unknownNames.add(path);
    else callsByName.set(path, (callsByName.get(path) ?? 0) + 1);
    push(v);
    return v.t;
  },
  call: (nargs: number, nres: number) => {
    const f = F();
    if (nargs < 0 || nargs + 1 > f.length) bug(`sos_call(${nargs}) with ${f.length} values`);
    const a = f.splice(f.length - nargs, nargs);
    const fv = f.pop()!;
    calls++;
    if (trace) console.log(`  call ${fv.t === 6 && fv.js ? (fv.js.name || 'api') : 'C'}(${a.map((x) => JSON.stringify(v2js(x))).join(', ')})`);
    let res: V[];
    try {
      if (fv.t !== 6) throw `attempt to call a ${['nil', 'boolean', 'number', 'number', 'string', 'table', 'function', 'object'][fv.t]} value`;
      res = fv.js ? fv.js(a) : callC(fv as V & { t: 6 }, a, nres, 'a C function');
    } catch (e) {
      if (e instanceof AppBug) throw e;
      push(s(String(e)));
      return 1;
    }
    if (nres !== -1) {
      res = res.slice(0, nres);
      while (res.length < nres) res.push(NIL);
    }
    res.forEach(push);
    return 0;
  },
  ref: () => {
    const v = F().pop() ?? bug('sos_ref with an empty frame');
    if (v.t === 0) return 0;
    if (refs.size >= 1024) bug('more than 1024 refs');
    refs.set(nextRef, v);
    return nextRef++;
  },
  unref: (r: number) => {
    if (r && !refs.delete(r)) bug(`sos_unref(${r}): no such ref`);
  },
  push_ref: (r: number) => {
    if (r && !refs.has(r)) bug(`sos_push_ref(${r}): no such ref`);
    const v = r ? refs.get(r)! : NIL;
    push(v);
    return v.t;
  },
  blit: (idx: number, px: number, x: number, y: number, w: number, h: number) => {
    const v = at(idx);
    if (w < 0 || h < 0) bug('sos_blit: negative size');
    bytes(px >>> 0, w * h * 2);
    blits++;
    return v.t === 7 && v.v.kind === 'canvas' ? 0 : -1;
  },
  frame: (px: number, w: number, h: number, x: number, y: number, dw: number, dh: number, flags: number) => {
    if (w < 1 || h < 1 || w > 1024 || h > 1024) bug(`sos_frame: ${w} x ${h} pixels`);
    if (flags & ~1) bug(`sos_frame: unknown flags ${flags}`);
    bytes(px >>> 0, w * h * 2);
    frameCalls++;
    return x < 0 || y < 0 || dw < 1 || dh < 1 || x + dw > 320 || y + dh > 218 ? -1 : 1;
  },
  file_read: (np: number, nl: number, off: number, buf: number, len: number) => {
    if (len >>> 0 > 256 * 1024) bug(`sos_file_read: ${len >>> 0} bytes`);
    cstr(np, nl);
    bytes(buf >>> 0, len >>> 0);
    const size = 32768, n = Math.max(0, Math.min(len >>> 0, size - (off >>> 0)));
    new Uint8Array(mem.buffer, buf >>> 0, n).fill(0);
    return n;
  },
  keys: () => heldKeys,
  millis: () => (millis += 30),
  log: (p: number, n: number) => out(`[app] ${cstr(p, n)}`),
  fail: (p: number, n: number) => {
    throw new AppBug(`sos_fail: ${cstr(p, n)}`);
  },
  host_abi: () => 1,
};
// --- Pocket Arcade: frames into a screenshot, card reads from the folder ---
const screen = new Uint8Array(320 * 218 * 3);
let lastFrameNoCopy = 0;
Object.assign(badgeImports, {
  frame: (px: number, w: number, h: number, x: number, y: number, dw: number, dh: number, flags: number) => {
    if (w < 1 || h < 1 || w > 1024 || h > 1024) bug(`sos_frame: ${w} x ${h} pixels`);
    if (flags & ~1) bug(`sos_frame: unknown flags ${flags}`);
    const src = bytes(px >>> 0, w * h * 2);
    frameCalls++;
    if (x < 0 || y < 0 || dw < 1 || dh < 1 || x + dw > 320 || y + dh > 218) return -1;
    // the badge takes it with no copy only when it is big-endian, unscaled, 64-byte aligned
    if ((flags & 1) && dw === w && dh === h && (px >>> 0) % 64 === 0 && (w * h * 2) % 64 === 0) lastFrameNoCopy++;
    for (let yy = 0; yy < dh; yy++) {
      const sy = Math.floor((yy * h) / dh);
      for (let xx = 0; xx < dw; xx++) {
        const sx = Math.floor((xx * w) / dw);
        const o = (sy * w + sx) * 2;
        const v = flags & 1 ? (src[o] << 8) | src[o + 1] : src[o] | (src[o + 1] << 8);
        const d = ((y + yy) * 320 + x + xx) * 3;
        screen[d] = (v >> 11) << 3;
        screen[d + 1] = ((v >> 5) & 63) << 2;
        screen[d + 2] = (v & 31) << 3;
      }
    }
    return 1;
  },
  file_read: (np: number, nl: number, off: number, buf: number, len: number) => {
    if (len >>> 0 > 256 * 1024) bug(`sos_file_read: ${len >>> 0} bytes`);
    const name = cstr(np, nl);
    const p = fileName(name);
    bytes(buf >>> 0, len >>> 0);
    if (!existsSync(p)) return -1;
    const all = readFileSync(p);
    const n = Math.max(0, Math.min(len >>> 0, all.length - (off >>> 0)));
    u8().set(all.subarray(off >>> 0, (off >>> 0) + n), buf >>> 0);
    return n;
  },
  millis: () => (millis += 16),
});

let millis = 0;
let heldKeys = 0;
let blits = 0;
let frameCalls = 0;
let delivered = 0;
const out = (l: string) => quiet || console.log(l);

let stdoutBuf = '';
const wasi = {
  fd_write: (fd: number, iovs: number, n: number, nw: number) => {
    const dv = new DataView(mem.buffer);
    let total = 0;
    for (let k = 0; k < n; k++) {
      const p = dv.getUint32(iovs + k * 8, true);
      const l = dv.getUint32(iovs + k * 8 + 4, true);
      stdoutBuf += cstr(p, l);
      total += l;
    }
    let nl;
    while ((nl = stdoutBuf.indexOf('\n')) >= 0) {
      out(`[${fd === 2 ? 'stderr' : 'stdout'}] ${stdoutBuf.slice(0, nl)}`);
      stdoutBuf = stdoutBuf.slice(nl + 1);
    }
    dv.setUint32(nw, total, true);
    return 0;
  },
  clock_time_get: (_id: number, _prec: bigint, p: number) => (new DataView(mem.buffer).setBigUint64(p, BigInt(Date.now()) * 1000000n, true), 0),
  random_get: (p: number, n: number) => (u8().set(crypto.getRandomValues(new Uint8Array(n)), p), 0),
  proc_exit: (c: number) => {
    throw new AppBug(`proc_exit(${c})`);
  },
  // Character device for stdout/stderr, so wasi-libc line-buffers them (as the badge should).
  fd_fdstat_get: (fd: number, p: number) => {
    if (fd > 2) return 8;
    const dv = new DataView(mem.buffer);
    for (let k = 0; k < 24; k++) dv.setUint8(p + k, 0);
    dv.setUint8(p, 2);
    return 0;
  },
  fd_close: () => 0,
  fd_seek: () => 70, // ESPIPE
  environ_sizes_get: (c: number, sz: number) => (new DataView(mem.buffer).setUint32(c, 0, true), new DataView(mem.buffer).setUint32(sz, 0, true), 0),
  environ_get: () => 0,
  args_sizes_get: (c: number, sz: number) => (new DataView(mem.buffer).setUint32(c, 0, true), new DataView(mem.buffer).setUint32(sz, 0, true), 0),
  args_get: () => 0,
};

// --- run: a script of ticks, keys, checks, screenshots and relaunches ---------
const wasmBytes = readFileSync(file);
let ex: Record<string, any> = {};

function run(what: string, f: () => number | void, frame: V[] = [], wantResults?: number) {
  frames.push([...frame]);
  try {
    const r = f();
    const fr = frames[frames.length - 1];
    const results = typeof r === 'number' && wantResults !== undefined ? r : 0;
    if (wantResults !== undefined && results > wantResults) problems.push(`${what}: returned ${results} results`);
    if (fr.length !== frame.length + results) problems.push(`${what}: left ${fr.length} values on its frame (${frame.length} + ${results} expected)`);
    return r;
  } catch (e) {
    if (e instanceof AppBug) problems.push(`${what}: ${e.message}`);
    else problems.push(`${what}: trapped: ${(e as Error).message}`);
  } finally {
    frames.pop();
  }
}
function deliver() {
  for (let n = 0; queue.length && n < 500; n++) {
    const p = queue.shift()!;
    if (p.f.t !== 6) continue;
    if (p.f.js) continue;
    frames.push([]);
    delivered++;
    try {
      callC(p.f as V & { t: 6 }, p.args, 0, `callback (${p.what})`);
    } catch (e) {
      problems.push(`callback (${p.what}): ${e instanceof AppBug ? e.message : 'trapped: ' + (e as Error).message}`);
    } finally {
      frames.pop();
    }
  }
}
function launch() {
  labels.clear();
  focusList = null;
  hint = '';
  queue.length = 0;
  refs.clear();
  const mod = new WebAssembly.Module(wasmBytes);
  const inst = new WebAssembly.Instance(mod, { badge: badgeImports, wasi_snapshot_preview1: wasi } as WebAssembly.Imports);
  ex = inst.exports as Record<string, any>;
  mem = ex.memory;
  table = ex.__indirect_function_table;
  if (ex.badge_abi && ex.badge_abi() !== 1) problems.push(`badge_abi() = ${ex.badge_abi()}`);
  run('_initialize', () => ex._initialize?.());
  run('on_start', () => ex.on_start());
  deliver();
}
function texts(): string[] {
  const t = [...labels.values()].filter((l) => l.shown).sort((a, b) => a.y - b.y || a.x - b.x).map((l) => l.text);
  if (focusList) {
    const n = tlen(focusList.items);
    for (let k = 0; k < n; k++) t.push(`${k === focusList.cursor ? '>' : ' '} ${listItemText(focusList.items.v.get(k + 1)!)}`);
  }
  if (hint) t.push(`[hint] ${hint}`);
  return t;
}
function png(path: string) {
  const W = 320, H = 218;
  const raw = Buffer.alloc((W * 3 + 1) * H);
  for (let y = 0; y < H; y++) {
    raw[y * (W * 3 + 1)] = 0;
    Buffer.from(screen.buffer, y * W * 3, W * 3).copy(raw, y * (W * 3 + 1) + 1);
  }
  const crcT = new Int32Array(256).map((_, n) => {
    let c = n;
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    return c;
  });
  const crc = (b: Buffer) => {
    let c = -1;
    for (const x of b) c = crcT[(c ^ x) & 255] ^ (c >>> 8);
    return (c ^ -1) >>> 0;
  };
  const chunk = (type: string, data: Buffer) => {
    const len = Buffer.alloc(4);
    len.writeUInt32BE(data.length);
    const td = Buffer.concat([Buffer.from(type), data]);
    const c = Buffer.alloc(4);
    c.writeUInt32BE(crc(td));
    return Buffer.concat([len, td, c]);
  };
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(W, 0);
  ihdr.writeUInt32BE(H, 4);
  ihdr[8] = 8;
  ihdr[9] = 2;
  writeFileSync(path, Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', ihdr), chunk('IDAT', deflateSync(raw)), chunk('IEND', Buffer.alloc(0))]));
}

const keyIds: Record<string, number> = { up: 0, down: 1, left: 2, right: 3, select: 4, back: 5 };
const mask = (k: string | undefined) => (k || '').split(',').filter(Boolean).reduce((m, n) => m | (1 << keyIds[n]), 0);
mkdirSync(SHOTS, { recursive: true });
launch();
type Step = { tick?: number; ms?: number; hold?: string; key?: string; shot?: string; expect?: string; absent?: string; exit?: boolean; log?: string; dump?: boolean; select_row?: string; leds?: boolean };
const steps: Step[] = SCRIPT ? JSON.parse(readFileSync(SCRIPT, 'utf8')) : [{ tick: 5 }];
let tickNo = 0;
for (const st of steps) {
  if (st.log) console.log(`--- ${st.log}`);
  if (st.tick) {
    heldKeys = mask(st.hold);
    for (let t = 0; t < st.tick; t++) {
      run(`on_tick #${++tickNo}`, () => ex.on_tick(st.ms ?? 30));
      deliver();
    }
    heldKeys = 0;
  }
  if (st.select_row) {
    // move the focused list to the first row containing the text, then press SELECT
    if (!focusList) problems.push(`select_row "${st.select_row}": no list on screen`);
    else {
      const n = tlen(focusList.items);
      let k = 0;
      while (k < n && !listItemText(focusList.items.v.get(k + 1)!).includes(st.select_row)) k++;
      if (k >= n) problems.push(`select_row: no row "${st.select_row}"`);
      else {
        focusList.cursor = k;
        run(`on_tick #${++tickNo}`, () => ex.on_tick(30));
        deliver();
        if (focusList.cb) later(focusList.cb, [k + 1, focusList.items.v.get(k + 1)!], 'list on_select');
        deliver();
        run(`on_tick #${++tickNo}`, () => ex.on_tick(30));   // the app acts on a choice at its next tick
        deliver();
      }
    }
  }
  if (st.key) {
    const L = focusList;
    if (L && (st.key === 'up' || st.key === 'down')) {
      const n = tlen(L.items);
      L.cursor = Math.max(0, Math.min(n - 1, L.cursor + (st.key === 'down' ? 1 : -1)));
    } else if (L && st.key === 'select') {
      if (L.cb) later(L.cb, [L.cursor + 1, L.items.v.get(L.cursor + 1)!], 'list on_select');
      deliver();
      run(`on_tick #${++tickNo}`, () => ex.on_tick(30));
      deliver();
    } else {
      const r = run(`on_key(${st.key})`, () => ex.on_key(keyIds[st.key!], 0));
      deliver();
      if (st.key === 'back' && r === 0) {
        console.log('[app] CANCEL left the app');
        run('on_exit', () => ex.on_exit?.());
        launch();
      }
    }
  }
  if (st.expect) {
    const all = [...texts(), ...toasts];
    if (!all.some((t) => t.includes(st.expect!))) problems.push(`expected "${st.expect}" on screen; have: ${all.join(' / ').slice(0, 400)}`);
  }
  if (st.absent) {
    const all = texts();
    if (all.some((t) => t.includes(st.absent!))) problems.push(`"${st.absent}" should not be on screen`);
  }
  if (st.dump) console.log(texts().map((t) => '    ' + t).join('\n'));
  if (st.leds) console.log('    leds ' + leds.map((l) => l.join(',')).join(' '));
  if (st.shot) {
    png(join(SHOTS, st.shot + '.png'));
    writeFileSync(join(SHOTS, st.shot + '.txt'), texts().join('\n') + '\n');
  }
  if (st.exit) {
    console.log('[sim] HOME: on_exit, then a fresh launch');
    run('on_exit', () => ex.on_exit?.());
    launch();
  }
}
if (ex.on_exit) run('on_exit', () => ex.on_exit());
if (stdoutBuf) out(`[stdout] ${stdoutBuf}`);
console.log(`sim: ${calls} API calls, ${delivered} callbacks, ${frameCalls} frames (${lastFrameNoCopy} no-copy), ${refs.size} refs held${unknownNames.size ? `, unknown names: ${[...unknownNames].join(' ')}` : ''}`);
for (const p of problems) console.log(`sim: PROBLEM: ${p}`);
process.exit(problems.length ? 1 : 0);
