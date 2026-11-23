// node --experimental-sqlite server/test.mjs
// Signs requests the way the badge server does (own Ed25519 key) and runs them
// against the worker with an in-memory SQLite standing in for D1.
import { DatabaseSync } from 'node:sqlite';
import { readFileSync } from 'node:fs';
import worker, { hooks, cleanName } from './worker.js';

const db = new DatabaseSync(':memory:');
db.exec(readFileSync(new URL('./schema.sql', import.meta.url), 'utf8'));
const D1 = {
  prepare(sql) {
    const st = db.prepare(sql.replace(/\?(\d+)/g, '?'));
    // D1 numbers its parameters (?1, ?2 ...); node:sqlite takes them in order of appearance
    const order = [...sql.matchAll(/\?(\d+)/g)].map((m) => Number(m[1]) - 1);
    return {
      bind: (...args) => {
        const vals = order.map((k) => args[k]);
        return {
          run: async () => st.run(...vals),
          all: async () => ({ results: st.all(...vals) }),
          first: async () => st.get(...vals) ?? null,
        };
      },
    };
  },
};
const env = { DB: D1 };

const kp = await crypto.subtle.generateKey('Ed25519', true, ['sign', 'verify']);
const pub = Buffer.from(await crypto.subtle.exportKey('raw', kp.publicKey)).toString('base64');
hooks.fetchKey = async () => ({ id: 'k1', key: pub });

const hex = (b) => Buffer.from(b).toString('hex');
let reqN = 0;
async function call(method, path, bodyObj, { user = 'u1', badge = '7K2Q9M', app = 'pocket-arcade', time = Date.now(), tamper = false, req: rid } = {}) {
  const url = `https://pocket-arcade.example.workers.dev${path}`;
  const body = bodyObj === undefined ? '' : JSON.stringify(bodyObj);
  const id = rid ?? `r${++reqN}`;
  const hash = hex(await crypto.subtle.digest('SHA-256', new TextEncoder().encode(body)));
  const signed = ['badge-http-v1', method, url, hash, app, badge, user, String(time), id].join('\n');
  const sig = Buffer.from(await crypto.subtle.sign('Ed25519', kp.privateKey, new TextEncoder().encode(signed))).toString('base64');
  const headers = {
    'x-badge-app': app, 'x-badge-id': badge, 'x-badge-user': user, 'x-badge-time': String(time),
    'x-badge-request': id, 'x-badge-key': 'k1', 'x-badge-signature': sig,
  };
  const sent = tamper ? JSON.stringify({ ...bodyObj, score: 4_999_999 }) : body;
  const res = await worker.fetch(new Request(url, { method, headers, body: method === 'GET' ? undefined : sent }), env);
  return { status: res.status, data: await res.json() };
}

let fails = 0;
const check = (name, ok) => { console.log((ok ? 'ok   ' : 'FAIL ') + name); if (!ok) fails++; };

let r = await call('POST', '/score', { score: 1200, stage: 2, epoch: 1, name: '@alice' }, { user: 'u1' });
check('signed score accepted', r.status === 200 && r.data.top[0].s === 1200 && r.data.me.r === 1);
r = await call('POST', '/score', { score: 900, stage: 1, epoch: 1, name: '@alice' }, { user: 'u1' });
check('lower score does not replace the best', r.data.top[0].s === 1200);
r = await call('POST', '/score', { score: 3000, stage: 4, epoch: 1, name: '@bob' }, { user: 'u2' });
check('ranking order', r.data.top[0].n === '@bob' && r.data.top[1].n === '@alice' && r.data.me.r === 1);
r = await call('GET', '/top?n=10', undefined, { user: 'u1' });
check('top + my rank', r.status === 200 && r.data.top.length === 2 && r.data.me.r === 2 && r.data.me.s === 1200);
r = await call('GET', '/top?n=10', undefined, { user: 'u9' });
check('player without a score: me is null', r.data.me === null);

r = await call('POST', '/score', { score: 100, stage: 1, epoch: 1 }, { tamper: true });
check('body changed after signing: refused', r.status === 401);
r = await call('POST', '/score', { score: 100, stage: 1, epoch: 1 }, { app: 'other-app' });
check('other app id: refused', r.status === 401);
r = await call('POST', '/score', { score: 100, stage: 1, epoch: 1 }, { time: Date.now() - 120_000 });
check('old request: refused', r.status === 401);
await call('GET', '/top', undefined, { req: 'same' });
r = await call('GET', '/top', undefined, { req: 'same' });
check('replayed request: refused', r.status === 401);
const plain = await worker.fetch(new Request('https://x.workers.dev/top'), env);
check('unsigned request: refused', plain.status === 401);
const card = await worker.fetch(new Request('https://fb.test/card.png'), { DB: D1 });
const png = new Uint8Array(await card.arrayBuffer());
check('card.png is public and a PNG', card.status === 200 && png[1] === 0x50 && png[2] === 0x4e && png[3] === 0x47);
const page = await (await worker.fetch(new Request('https://fb.test/share?s=4200<b>'), { DB: D1 })).text();
check('share page: public, card tags, the score, nothing injected',
  page.includes('summary_large_image') && page.includes('https://fb.test/card.png') && page.includes('Tank Siege: 0 ') && !page.includes('<b>'));
const page2 = await (await worker.fetch(new Request('https://fb.test/share?s=4200'), { DB: D1 })).text();
check('share page names the score', page2.includes('Tank Siege: 4200 '));
r = await call('POST', '/score', { score: 99_000_000, stage: 1, epoch: 1 });
check('absurd score: refused', r.status === 400);
r = await call('POST', '/score', { score: 10, stage: 100 });
check('stage out of range: refused', r.status === 400);
r = await call('POST', '/score', { score: 50, stage: 3, name: '@carol' }, { user: 'u7' });
check('Tank Siege score without an epoch: accepted', r.status === 200);
check('name cleaning', cleanName('@verylongname_1234567890\u0007') === '@verylongname_12' && cleanName('') === 'anon');

console.log(fails ? `FAILED ${fails}` : 'all passed');
process.exit(fails ? 1 : 0);
