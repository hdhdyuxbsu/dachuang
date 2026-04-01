# 公网部署指南（网页可公网访问 + 非同局域网控制 ESP32）

本指南对应当前仓库代码，目标是：
- 网页部署到公网可访问
- 不在 ESP32 同一局域网也能获取数据和下发指令
- 尽量复用现有页面逻辑（直接移植当前 HTML）

## 1. 当前架构说明

当前页面的数据和控制链路是：
- 数据读取：优先走 OneNet（公网可访问）
- 控制下发：页面调用 ESP32 的 `/api/control` 和 `/api/plant`

所以公网场景真正要解决的是控制下发路径。

本仓库已提供可用中转方案：
- `docs/worker.js` 新增了两个路由
- `/onenet?url=...`：OneNet CORS 代理
- `/esp32/...`：ESP32 API 中转（如 `/esp32/api/control`）

你只需要让 Worker 能访问到 ESP32（通过内网穿透地址），页面就可以在任意公网网络下控制设备。

## 2. 步骤总览

1. 部署静态网页到 GitHub Pages（或任意静态托管）
2. 部署 Cloudflare Worker（使用 `docs/worker.js`）
3. 给 ESP32 本地 HTTP 服务做内网穿透（拿到公网地址）
4. 在 Worker 中配置 `ESP32_BASE_URL` 为内网穿透地址
5. 打开公网网页，填写 `https://你的worker域名/esp32`
6. 用手机 4G 验证读取与控制

## 3. 部署网页（GitHub Pages）

建议使用 `docs` 目录作为发布源：
- 页面文件：`docs/index.html`
- Worker 文件：`docs/worker.js`（此文件部署到 Cloudflare，不是 Pages）

GitHub Pages 设置：
1. 进入仓库 Settings -> Pages
2. Source 选择 Deploy from a branch
3. Branch 选 `main`（或你的发布分支）
4. Folder 选 `/docs`
5. 保存后等待生成公网地址

## 4. 部署 Worker

1. 打开 Cloudflare Dashboard -> Workers & Pages -> Create
2. 创建 Worker（例如命名 `greenhouse-gateway`）
3. 进入 Edit Code，将 `docs/worker.js` 全部内容粘贴覆盖
4. Deploy
5. 得到 Worker 地址，例如：
   - `https://greenhouse-gateway.xxx.workers.dev`

## 5. 让 Worker 能访问 ESP32（内网穿透）

ESP32 通常在内网，公网无法直接访问。需要给 ESP32 HTTP 服务做穿透。

可选工具：
- ngrok
- frp
- 花生壳
- Cloudflare Tunnel（若你有中转主机）

要求：
- 穿透后拿到一个公网 HTTPS 地址，能转发到 `http://ESP32局域网IP:80`
- 例如得到：`https://abc123.ngrok-free.app`

本地先自测：
- 浏览器访问 `https://abc123.ngrok-free.app/api/data`
- 能返回 JSON 说明穿透正常

## 6. 配置 Worker 的 ESP32_BASE_URL

在 Worker 设置中添加环境变量：
- Name: `ESP32_BASE_URL`
- Value: 你的穿透地址（例如 `https://abc123.ngrok-free.app`）

配置后重新 Deploy Worker。

说明：
- 你也可以不配环境变量，而是在请求里动态传 `x-esp32-base`，但生产建议固定环境变量。

## 7. 页面连接方式

打开你的公网网页后，在页面输入框填写：
- `https://greenhouse-gateway.xxx.workers.dev/esp32`

页面会把控制请求发到：
- `/esp32/api/control`
- `/esp32/api/plant`

Worker 再转发到你的 ESP32。

你也可以直接把这个地址写进链接参数：
- `?esp32=https://greenhouse-gateway.xxx.workers.dev/esp32`

## 8. 验证清单（建议按顺序）

1. 网页可打开（手机 4G）
2. 面板数据能刷新（OneNet）
3. 输入网关地址后状态显示 ESP32 已连接
4. 切换手动模式，拖动风扇/水泵/舵机，ESP32 端有响应
5. 设置监控对象（`/api/plant`）成功

如果第 2 步成功但第 4 步失败：
- 通常是内网穿透或 Worker 的 `ESP32_BASE_URL` 配置问题

## 9. 常见问题

### Q1: 为什么数据能看，但下发失败？
因为数据来自 OneNet（公网），控制要到 ESP32 本地 API；下发失败通常是穿透链路没通。

### Q2: Worker 返回 502？
表示 Worker 到上游失败。检查：
- `ESP32_BASE_URL` 是否正确
- 穿透是否在线
- ESP32 是否还在同一局域网地址

### Q3: 需要改 ESP32 固件吗？
这个方案不强制改固件，复用当前 `/api/control` 和 `/api/plant` 即可。

## 10. 安全建议（上线必做）

1. 在 Worker 中增加鉴权（例如固定 Token）
2. 限制调用来源域名（Access-Control-Allow-Origin 不建议长期用 `*`）
3. 不要把内网穿透地址公开到社交平台
4. 定期更换穿透隧道凭证

---

如果你希望彻底去掉内网穿透，也可以做下一步升级：
- 改成“网页 -> 云平台 API -> OneNet 下行 -> ESP32 MQTT 订阅执行”
- 这样控制链路完全云化，不依赖本地 HTTP
