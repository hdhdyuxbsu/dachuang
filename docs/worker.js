/*
 * Cloudflare Worker — 公网网关
 *
 * 路由说明：
 * 1) /onenet?url=https://iot-api.heclouds.com/xxx
 *    - 保留原有 OneNet CORS 代理能力
 *
 * 2) /esp32/<path>
 *    - 例如 POST /esp32/api/control, /esp32/api/plant
 *    - Worker 会把请求转发到 ESP32_BASE_URL + <path>
 *
 * 配置方式（任选其一）：
 * - 推荐：在 Worker 环境变量中配置 ESP32_BASE_URL（例如 https://abc.ngrok-free.app）
 * - 备用：请求携带 header x-esp32-base 或 query 参数 base
 */

const CORS_HEADERS = {
  'Access-Control-Allow-Origin': '*',
  'Access-Control-Allow-Methods': 'GET, POST, PUT, PATCH, DELETE, OPTIONS',
  'Access-Control-Allow-Headers': 'authorization, content-type, x-esp32-base',
  'Access-Control-Max-Age': '86400',
};

function withCors(headers) {
  const h = new Headers(headers || {});
  Object.keys(CORS_HEADERS).forEach((key) => h.set(key, CORS_HEADERS[key]));
  return h;
}

function jsonResponse(data, status = 200) {
  return new Response(JSON.stringify(data), {
    status,
    headers: withCors({ 'Content-Type': 'application/json; charset=utf-8' }),
  });
}

function normalizeBaseUrl(input) {
  const raw = String(input || '').trim();
  if (!raw) return '';
  try {
    const u = new URL(raw);
    if (u.protocol !== 'http:' && u.protocol !== 'https:') return '';
    const path = u.pathname.replace(/\/+$/, '');
    return `${u.protocol}//${u.host}${path}`;
  } catch {
    return '';
  }
}

function appendCorsToResponse(resp) {
  const headers = withCors(resp.headers);
  return new Response(resp.body, {
    status: resp.status,
    statusText: resp.statusText,
    headers,
  });
}

async function handleOneNetProxy(requestUrl, request) {
  const target = requestUrl.searchParams.get('url');
  if (!target) {
    return jsonResponse({ error: 'Missing url parameter' }, 400);
  }

  let targetUrl;
  try {
    targetUrl = new URL(target);
  } catch {
    return jsonResponse({ error: 'Invalid target url' }, 400);
  }

  if (targetUrl.hostname !== 'iot-api.heclouds.com') {
    return jsonResponse({ error: 'Only iot-api.heclouds.com is allowed for /onenet' }, 403);
  }

  const headers = new Headers();
  if (request.headers.has('authorization')) {
    headers.set('authorization', request.headers.get('authorization'));
  }
  if (request.headers.has('content-type')) {
    headers.set('content-type', request.headers.get('content-type'));
  }

  try {
    const upstream = await fetch(targetUrl.toString(), {
      method: request.method,
      headers,
      body: request.method === 'GET' || request.method === 'HEAD' ? undefined : request.body,
      redirect: 'follow',
    });
    return appendCorsToResponse(upstream);
  } catch (e) {
    return jsonResponse({ error: 'OneNet proxy failed', detail: String(e && e.message ? e.message : e) }, 502);
  }
}

async function handleEsp32Proxy(requestUrl, request, env) {
  const rawBase =
    env.ESP32_BASE_URL ||
    request.headers.get('x-esp32-base') ||
    requestUrl.searchParams.get('base') ||
    '';

  const base = normalizeBaseUrl(rawBase);
  if (!base) {
    return jsonResponse({
      error: 'ESP32 base url is not configured',
      hint: 'Set Worker env ESP32_BASE_URL or send header x-esp32-base',
    }, 400);
  }

  const suffixPath = requestUrl.pathname.replace(/^\/esp32/, '') || '/';
  const target = `${base}${suffixPath}${requestUrl.search}`;

  const headers = new Headers();
  if (request.headers.has('content-type')) {
    headers.set('content-type', request.headers.get('content-type'));
  }
  if (request.headers.has('authorization')) {
    headers.set('authorization', request.headers.get('authorization'));
  }

  try {
    const upstream = await fetch(target, {
      method: request.method,
      headers,
      body: request.method === 'GET' || request.method === 'HEAD' ? undefined : request.body,
      redirect: 'follow',
    });
    return appendCorsToResponse(upstream);
  } catch (e) {
    return jsonResponse({ error: 'ESP32 proxy failed', detail: String(e && e.message ? e.message : e) }, 502);
  }
}

export default {
  async fetch(request, env) {
    if (request.method === 'OPTIONS') {
      return new Response(null, { headers: withCors() });
    }

    const requestUrl = new URL(request.url);

    if (requestUrl.pathname === '/onenet') {
      return handleOneNetProxy(requestUrl, request);
    }

    if (requestUrl.pathname.startsWith('/esp32/')) {
      return handleEsp32Proxy(requestUrl, request, env || {});
    }

    return jsonResponse({
      ok: true,
      message: 'Worker is running',
      routes: ['/onenet?url=https://iot-api.heclouds.com/...', '/esp32/api/data', '/esp32/api/control', '/esp32/api/plant'],
    });
  },
};
