// Pocket Arcade leaderboard (Tank Siege best scores): a Cloudflare Worker with a D1 table.
// Every request must come through the badge server, which signs it (X-Badge-* headers).
//   POST /score  {score, stage, name}  -> keeps the player's best (Tank Siege)
//   GET  /top?n=10                            -> {top: [{r, n, s, st}], me: {r, s} | null}
//   GET  /share?s=..   (public) a page for X with a picture card; GET /card.png (public) the picture
// See https://solana.com/breakpoint/badge/apps/docs (http: "What your server receives").

import { CARD_B64 } from './card.js';

export const APP = 'pocket-arcade';
const CARD = Uint8Array.from(atob(CARD_B64), (c) => c.charCodeAt(0));
const SERVER = 'https://badge-api.solana.com';
const MAX_SCORE = 9_999_999;

const keys = new Map(); // key id -> CryptoKey
const seen = new Map(); // X-Badge-Request -> time
let fetching = null;
let fetchedAt = 0;

export const hooks = { fetchKey: async () => (await fetch(`${SERVER}/api/http/key`)).json(), now: () => Date.now() };

async function badgeKey(id) {
  if (!keys.has(id) && !fetching && hooks.now() - fetchedAt > 60_000) {
    fetchedAt = hooks.now();
    fetching = (async () => {
      const k = await hooks.fetchKey();
      const raw = Uint8Array.from(atob(k.key), (c) => c.charCodeAt(0));
      keys.set(k.id, await crypto.subtle.importKey('raw', raw, 'Ed25519', false, ['verify']));
    })()
      .catch(() => { fetchedAt = 0; })
      .finally(() => { fetching = null; });
  }
  if (!keys.has(id) && fetching) await fetching;
  return keys.get(id) ?? null;
}

function once(id, time) {
  for (const [k, t] of seen) if (hooks.now() - t > 60_000) seen.delete(k);
  if (seen.has(id)) return false;
  seen.set(id, time);
  return true;
}

const hex = (buf) => [...new Uint8Array(buf)].map((b) => b.toString(16).padStart(2, '0')).join('');

/** The badge and player behind a request from Pocket Arcade, or null. */
export async function fromBadge(req, body) {
  const h = (n) => req.headers.get(n) ?? '';
  const time = Number(h('x-badge-time'));
  const id = h('x-badge-request');
  if (h('x-badge-app') !== APP || !(Math.abs(hooks.now() - time) < 60_000) || !id) return null;
  const key = await badgeKey(h('x-badge-key'));
  if (!key) return null;
  const hash = await crypto.subtle.digest('SHA-256', new TextEncoder().encode(body));
  const signed = ['badge-http-v1', req.method, req.url, hex(hash), APP, h('x-badge-id'),
    h('x-badge-user'), h('x-badge-time'), id].join('\n');
  let sig;
  try { sig = Uint8Array.from(atob(h('x-badge-signature')), (c) => c.charCodeAt(0)); } catch { return null; }
  if (!(await crypto.subtle.verify('Ed25519', key, sig, new TextEncoder().encode(signed)))) return null;
  if (!once(id, time)) return null;
  return { badge: h('x-badge-id'), user: h('x-badge-user') };
}

const json = (data, status = 200) => new Response(JSON.stringify(data), {
  status, headers: { 'content-type': 'application/json' },
});

/** A name safe to show on a 320 px screen: printable ASCII, 16 chars at most. */
export function cleanName(v) {
  const s = String(v ?? '').replace(/[^\x20-\x7E]/g, '').trim().slice(0, 16);
  return s || 'anon';
}

const int = (v, lo, hi) => Number.isInteger(v) && v >= lo && v <= hi;

export default {
  async fetch(req, env) {
    const url = new URL(req.url);
    // Public: the X crawler reads these, not a badge.
    if (req.method === 'GET' && url.pathname === '/card.png') {
      return new Response(CARD, { headers: { 'content-type': 'image/png', 'cache-control': 'public, max-age=86400' } });
    }
    if (req.method === 'GET' && url.pathname === '/share') {
      const score = Math.max(0, Math.min(MAX_SCORE, Math.floor(Number(url.searchParams.get('s')) || 0)));
      const title = `Tank Siege: ${score} in Pocket Arcade on the Solana Breakpoint badge`;
      const img = `${url.origin}/card.png`;
      const html = `<!doctype html><html><head><meta charset="utf-8"><title>${title}</title>
<meta name="twitter:card" content="summary_large_image"><meta property="og:title" content="${title}">
<meta property="og:description" content="16 free Game Boy games and Tank Siege, made for the badge."><meta property="og:image" content="${img}">
<meta name="twitter:image" content="${img}"><meta http-equiv="refresh" content="0; url=https://solana.com/breakpoint">
</head><body><a href="https://solana.com/breakpoint">solana.com/breakpoint</a></body></html>`;
      return new Response(html, { headers: { 'content-type': 'text/html; charset=utf-8', 'cache-control': 'public, max-age=3600' } });
    }
    const body = req.method === 'GET' || req.method === 'HEAD' ? '' : await req.text();
    const who = await fromBadge(req, body);
    if (!who) return json({ error: 'not from a badge' }, 401);

    if (req.method === 'POST' && url.pathname === '/score') {
      let d;
      try { d = JSON.parse(body); } catch { return json({ error: 'bad json' }, 400); }
      if (d.epoch === undefined) d.epoch = 1;          // Tank Siege has stages only
      if (!int(d.score, 1, MAX_SCORE) || !int(d.stage, 1, 99) || !int(d.epoch, 1, 999)) {
        return json({ error: 'bad score' }, 400);
      }
      // keep the best score per player (one row per badge owner)
      await env.DB.prepare(
        `INSERT INTO scores (user, badge, name, score, stage, epoch, at) VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7)
         ON CONFLICT(user) DO UPDATE SET badge = ?2, name = ?3, score = ?4, stage = ?5, epoch = ?6, at = ?7
         WHERE excluded.score > scores.score`,
      ).bind(who.user, who.badge, cleanName(d.name), d.score, d.stage, d.epoch, Date.now()).run();
      return json(await board(env, who.user, 10));
    }

    if (req.method === 'GET' && url.pathname === '/top') {
      const n = Math.min(10, Math.max(1, Number(url.searchParams.get('n')) || 10));
      return json(await board(env, who.user, n));
    }
    return json({ error: 'not found' }, 404);
  },
};

async function board(env, user, n) {
  const top = await env.DB.prepare(
    'SELECT name, score, stage, epoch FROM scores ORDER BY score DESC, at ASC LIMIT ?1',
  ).bind(n).all();
  const rows = (top.results ?? []).map((r, i) => ({ r: i + 1, n: r.name, s: r.score, st: r.stage, e: r.epoch }));
  const mine = await env.DB.prepare('SELECT score FROM scores WHERE user = ?1').bind(user).first();
  let me = null;
  if (mine) {
    const above = await env.DB.prepare('SELECT COUNT(*) AS c FROM scores WHERE score > ?1').bind(mine.score).first();
    me = { r: (above?.c ?? 0) + 1, s: mine.score };
  }
  return { top: rows, me };
}
