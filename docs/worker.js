/*
 * Cloudflare Worker — OneNet CORS 代理
 *
 * 部署步骤：
 * 1. 打开 https://dash.cloudflare.com/ 注册/登录（免费）
 * 2. 左侧菜单点 "Workers & Pages" → "Create"
 * 3. 选 "Create Worker"，取名如 "onenet-proxy"，点 "Deploy"
 * 4. 点 "Edit Code"，把这个文件的全部内容粘贴进去，点 "Deploy"
 * 5. 得到的 URL 类似：https://onenet-proxy.你的账号.workers.dev
 * 6. 把这个 URL 填到 index.html 的 WORKER_URL 变量中
 */

export default {
  async fetch(request) {
    // 处理 CORS 预检请求
    if (request.method === 'OPTIONS') {
      return new Response(null, {
        headers: {
          'Access-Control-Allow-Origin': '*',
          'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
          'Access-Control-Allow-Headers': 'authorization, content-type',
          'Access-Control-Max-Age': '86400',
        }
      });
    }

    // 从 URL 参数获取目标地址
    const url = new URL(request.url);
    const target = url.searchParams.get('url');
    if (!target) {
      return new Response(JSON.stringify({ error: 'Missing url parameter' }), {
        status: 400,
        headers: { 'Content-Type': 'application/json', 'Access-Control-Allow-Origin': '*' }
      });
    }

    // 转发请求到 OneNet，保留 authorization 头
    const headers = new Headers();
    if (request.headers.has('authorization')) {
      headers.set('authorization', request.headers.get('authorization'));
    }

    try {
      const resp = await fetch(target, { headers });
      const body = await resp.text();
      return new Response(body, {
        status: resp.status,
        headers: {
          'Content-Type': 'application/json',
          'Access-Control-Allow-Origin': '*',
          'Access-Control-Allow-Headers': 'authorization, content-type',
        }
      });
    } catch (e) {
      return new Response(JSON.stringify({ error: e.message }), {
        status: 502,
        headers: { 'Content-Type': 'application/json', 'Access-Control-Allow-Origin': '*' }
      });
    }
  }
};
