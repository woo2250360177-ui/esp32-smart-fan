#pragma once
// SmartFan IoT Platform dashboard, served directly from ESP32 PROGMEM.
const char PAGE[] PROGMEM = R"SFHTML(
<!doctype html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#0b1220">
<title>SmartFan IoT Platform</title>
<style>
:root{color-scheme:dark;--bg:#0b1220;--side:#0e1727;--panel:#121e30;--panel2:#0f1a2b;--line:#24344b;--text:#e8eef8;--muted:#8fa2bb;--cyan:#55d6d0;--blue:#77aaff;--amber:#f2bd68;--red:#f07878;--green:#75d6a6}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font:14px/1.55 Inter,ui-sans-serif,system-ui,-apple-system,"Segoe UI",sans-serif}
button,input,select{font:inherit}
button{cursor:pointer}
.shell{min-height:100vh;display:grid;grid-template-columns:236px minmax(0,1fr)}
.sidebar{background:var(--side);border-right:1px solid var(--line);padding:24px 16px;display:flex;flex-direction:column;gap:28px}
.brand{display:flex;align-items:center;gap:12px;padding:0 8px}
.brand-mark{width:38px;height:38px;border-radius:11px;background:linear-gradient(145deg,#1c777c,#234e75);display:grid;place-items:center;color:#eaffff;font-weight:800;letter-spacing:-.05em}
.brand-name{font-size:15px;font-weight:750;letter-spacing:.02em}
.brand-sub{font-size:10px;color:var(--muted);letter-spacing:.16em;margin-top:2px}
.nav-label{font-size:10px;letter-spacing:.17em;color:#657b96;padding:0 12px 8px}
.nav{display:grid;gap:6px}
.nav-btn{border:1px solid transparent;background:transparent;color:#9fb0c6;text-align:left;padding:11px 12px;border-radius:9px;display:flex;align-items:center;gap:11px}
.nav-btn:hover{background:#17263a;color:var(--text)}
.nav-btn.active{background:#17313c;border-color:#28545a;color:#a8f0e8}
.nav-icon{width:20px;text-align:center;font-size:16px}
.side-foot{margin-top:auto;border:1px solid var(--line);background:#101c2d;border-radius:12px;padding:13px}
.side-foot strong{display:block;font-size:12px;margin:5px 0}
.side-foot small{font-size:11px;color:var(--muted)}
.edge-dot{display:inline-block;width:7px;height:7px;background:var(--cyan);border-radius:50%;margin-right:7px;box-shadow:0 0 10px #55d6d066}
.main{min-width:0;display:flex;flex-direction:column}
.topbar{height:68px;border-bottom:1px solid var(--line);display:flex;align-items:center;justify-content:space-between;padding:0 30px;gap:18px;background:#0d1625}
.crumb{font-size:12px;color:var(--muted)}
.crumb b{color:var(--text);font-weight:600}
.top-actions{display:flex;align-items:center;gap:12px}
.status-pill{border:1px solid var(--line);border-radius:99px;padding:6px 11px;font-size:11px;color:var(--amber);white-space:nowrap}
.status-pill.online{color:var(--green);border-color:#285d50;background:#102b27}
.status-pill.offline{color:var(--amber);border-color:#604b2e;background:#2a2115}
.local-tag{font-size:11px;color:#9db0c8;background:#172338;padding:6px 10px;border-radius:6px}
.content{width:100%;max-width:1500px;margin:0 auto;padding:28px 30px 34px}
.page{display:none}
.page.active{display:block}
.page-head{display:flex;justify-content:space-between;align-items:flex-end;gap:16px;margin-bottom:22px}
.eyebrow{font-size:10px;color:var(--cyan);letter-spacing:.18em;text-transform:uppercase}
h1{font-size:25px;line-height:1.2;margin:7px 0 6px;letter-spacing:-.025em}
h2{font-size:15px;margin:0;font-weight:650}
h3{font-size:13px;margin:0;font-weight:650}
.subhead{font-size:12px;color:var(--muted);margin:0}
.updated{font-size:11px;color:var(--muted);text-align:right}
.grid{display:grid;gap:14px}
.kpi-grid{grid-template-columns:repeat(4,minmax(0,1fr));margin-bottom:14px}
.panel{background:var(--panel);border:1px solid var(--line);border-radius:12px;padding:18px;min-width:0}
.kpi{position:relative;overflow:hidden;min-height:118px}
.kpi:after{content:"";position:absolute;right:-24px;top:-30px;width:90px;height:90px;border-radius:50%;background:#ffffff05}
.kpi-label{font-size:11px;color:var(--muted)}
.kpi-value{font-size:30px;line-height:1.25;font-weight:700;letter-spacing:-.04em;margin:12px 0 5px;font-variant-numeric:tabular-nums}
.kpi-unit{font-size:13px;color:var(--muted);font-weight:500;letter-spacing:0}
.kpi-foot{font-size:10px;color:#71869f}
.overview-grid{grid-template-columns:minmax(0,1.7fr) minmax(260px,1fr);align-items:stretch}
.panel-head{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:15px}
.panel-note{font-size:10px;color:var(--muted)}
.chart{width:100%;height:225px;display:block}
.chart.small{height:180px}
.legend{display:flex;gap:12px;flex-wrap:wrap;font-size:10px;color:var(--muted)}
.legend span:before{content:"";display:inline-block;width:7px;height:7px;border-radius:50%;background:var(--blue);margin-right:5px}
.legend span:nth-child(2):before{background:var(--cyan)}
.legend span:nth-child(3):before{background:var(--amber)}
.device-state{display:grid;gap:0}
.state-row{display:flex;justify-content:space-between;align-items:center;gap:12px;padding:12px 0;border-bottom:1px solid #223149;font-size:12px}
.state-row:last-child{border-bottom:0}
.state-row span{color:var(--muted)}
.state-row b{font-weight:600;text-align:right}
.good{color:var(--green)!important}.warn{color:var(--amber)!important}.bad{color:var(--red)!important}
.notice{margin-top:14px;padding:11px 12px;border:1px solid #28534e;background:#102722;border-radius:8px;font-size:11px;color:#a8e8d5}
.notice.warn{border-color:#624d2f;background:#292116;color:#f1c47d}
.notice.bad{border-color:#633d45;background:#2a171e;color:#f49a9a}
.control-grid{grid-template-columns:minmax(0,1.2fr) minmax(280px,.8fr)}
.control-group{margin-top:20px}
.control-label{font-size:11px;color:var(--muted);margin-bottom:9px}
.btn-row{display:flex;gap:8px;flex-wrap:wrap}
.btn{border:1px solid #344862;background:#1a2a40;color:#dbe7f7;border-radius:8px;padding:10px 14px;font-size:12px}
.btn:hover{border-color:var(--cyan)}
.btn.active{background:#16413f;border-color:#2d817a;color:#aaf3e8}
.btn.primary{background:#174b4b;border-color:#2c7774;color:#c2fff6}
.btn.danger{background:#38232a;border-color:#74414a;color:#ffb6b6}
.btn:disabled{opacity:.35;cursor:not-allowed}
.control-tip{font-size:11px;color:var(--muted);margin-top:14px}
.fan-wrap{display:flex;align-items:center;gap:20px;padding:15px 0}
.fan-icon{width:76px;height:76px;border:1px solid #2a6063;background:#14343a;border-radius:50%;display:grid;place-items:center;font-size:35px;color:var(--cyan);flex:none}
.fan-icon.running span{display:block;animation:spin 2s linear infinite}
@keyframes spin{to{transform:rotate(360deg)}}
.fan-meta strong{font-size:23px;display:block;font-variant-numeric:tabular-nums}
.fan-meta small{font-size:11px;color:var(--muted)}
.metrics-two{grid-template-columns:repeat(2,minmax(0,1fr))}
.mini-metric{padding:13px;background:var(--panel2);border:1px solid #1e3048;border-radius:9px}
.mini-metric span{display:block;color:var(--muted);font-size:10px}
.mini-metric b{display:block;font-size:19px;margin-top:5px;font-variant-numeric:tabular-nums}
.table-tools{display:flex;justify-content:space-between;align-items:center;gap:12px;flex-wrap:wrap;margin-bottom:14px}
.tool-left,.tool-right{display:flex;gap:8px;align-items:center;flex-wrap:wrap}
select{background:#101b2b;color:var(--text);border:1px solid #30435d;border-radius:7px;padding:8px 10px;font-size:11px}
.table-wrap{overflow:auto;border:1px solid var(--line);border-radius:9px}
table{width:100%;border-collapse:collapse;min-width:880px;font-size:11px}
th{background:#17253a;color:#a9bbd1;text-align:left;font-weight:600;white-space:nowrap}
th,td{padding:10px 11px;border-bottom:1px solid #23334a}
td{color:#d6e1ef;font-variant-numeric:tabular-nums}
tbody tr:hover{background:#17263a}
tbody tr:last-child td{border-bottom:0}
.table-empty{text-align:center;color:var(--muted);padding:28px}
.pagination{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-top:13px;font-size:11px;color:var(--muted)}
.pager{display:flex;align-items:center;gap:8px}
.pager .btn{padding:6px 10px}
.progress{height:5px;background:#1a2a40;border-radius:9px;overflow:hidden;margin-top:12px}
.progress span{display:block;height:100%;background:var(--cyan);width:0}
.analysis-grid{grid-template-columns:repeat(2,minmax(0,1fr))}
.strategy-card{border-top:2px solid var(--blue)}
.strategy-card.trend{border-top-color:var(--amber)}
.strategy-title{display:flex;align-items:center;justify-content:space-between;margin-bottom:16px}
.strategy-title b{font-size:14px}
.strategy-value{font-size:28px;font-weight:700;margin:8px 0;font-variant-numeric:tabular-nums}
.strategy-desc{font-size:11px;color:var(--muted);min-height:36px}
.diagnostic-grid{grid-template-columns:repeat(2,minmax(0,1fr))}
.diagnostic-item{display:flex;justify-content:space-between;gap:12px;padding:13px 0;border-bottom:1px solid #23334a;font-size:12px}
.diagnostic-item:last-child{border-bottom:0}
.diagnostic-item span{color:var(--muted)}
.footer{padding:0 30px 22px;color:#637991;font-size:10px}
iframe{display:none}
@media(max-width:1050px){.shell{grid-template-columns:205px minmax(0,1fr)}.content{padding:22px 20px}.topbar{padding:0 20px}.kpi-grid{grid-template-columns:repeat(2,minmax(0,1fr))}.overview-grid{grid-template-columns:1fr}}
@media(max-width:700px){.shell{grid-template-columns:1fr}.sidebar{padding:12px;gap:12px;border-right:0;border-bottom:1px solid var(--line)}.brand{padding:0 4px}.nav{grid-template-columns:repeat(5,minmax(0,1fr));gap:4px}.nav-label,.side-foot{display:none}.nav-btn{padding:9px 4px;justify-content:center;flex-direction:column;gap:3px;font-size:9px}.nav-icon{font-size:15px}.topbar{height:54px;padding:0 14px}.content{padding:18px 12px}.page-head{align-items:flex-start;flex-direction:column}.updated{text-align:left}.kpi-grid{gap:8px}.kpi{padding:13px;min-height:105px}.kpi-value{font-size:24px}.control-grid,.analysis-grid,.diagnostic-grid{grid-template-columns:1fr}.panel{padding:14px}.chart{height:190px}.footer{padding:0 12px 18px}.local-tag{display:none}}
@media(prefers-reduced-motion:reduce){.fan-icon.running span{animation:none}}
</style>
</head>
<body>
<div class="shell">
<aside class="sidebar">
  <div class="brand"><div class="brand-mark">SF</div><div><div class="brand-name">SmartFan</div><div class="brand-sub">IOT PLATFORM</div></div></div>
  <div>
    <div class="nav-label">MONITORING CENTER</div>
    <nav class="nav" aria-label="主导航">
      <button class="nav-btn active" type="button" data-page="overview"><span class="nav-icon">▦</span><span>设备总览</span></button>
      <button class="nav-btn" type="button" data-page="control"><span class="nav-icon">◉</span><span>设备控制</span></button>
      <button class="nav-btn" type="button" data-page="experiments"><span class="nav-icon">▤</span><span>实验数据</span></button>
      <button class="nav-btn" type="button" data-page="analysis"><span class="nav-icon">⌁</span><span>算法分析</span></button>
      <button class="nav-btn" type="button" data-page="diagnostics"><span class="nav-icon">⌘</span><span>设备诊断</span></button>
    </nav>
  </div>
  <div class="side-foot"><small>DEVICE CONNECTION</small><strong><span class="edge-dot"></span>ESP32 EDGE NODE</strong><small>本地 Wi-Fi · 独立运行</small></div>
</aside>
<main class="main">
  <header class="topbar">
    <div class="crumb">SmartFan / <b id="crumbTitle">设备总览</b></div>
    <div class="top-actions"><span class="local-tag">LOCAL EDGE MODE</span><span id="connection" class="status-pill">等待设备连接</span></div>
  </header>
  <div class="content">
    <section class="page active" id="page-overview">
      <div class="page-head"><div><div class="eyebrow">REAL-TIME MONITORING / 01</div><h1>设备总览</h1><p class="subhead">实时掌握温度、风扇运行状态与控制策略</p></div><div class="updated">最近更新 <b id="updatedAt">--:--:--</b><br>设备编号 <b>SF-ESP32-01</b></div></div>
      <div class="grid kpi-grid">
        <article class="panel kpi"><div class="kpi-label">实时温度 / TEMPERATURE</div><div class="kpi-value" id="temp">-- <span class="kpi-unit">°C</span></div><div class="kpi-foot" id="filtered">平滑温度 --</div></article>
        <article class="panel kpi"><div class="kpi-label">风扇转速 / FAN SPEED</div><div class="kpi-value" id="rpm">-- <span class="kpi-unit">RPM</span></div><div class="kpi-foot">IO33 · 每转 2 脉冲估算</div></article>
        <article class="panel kpi"><div class="kpi-label">PWM 输出 / OUTPUT</div><div class="kpi-value" id="pwm">-- <span class="kpi-unit">%</span></div><div class="kpi-foot" id="levelText">目标档位 --</div></article>
        <article class="panel kpi"><div class="kpi-label">运行模式 / MODE</div><div class="kpi-value" id="modeText" style="font-size:22px">等待</div><div class="kpi-foot" id="rate">升温速率 --</div></article>
      </div>
      <div class="grid overview-grid">
        <article class="panel"><div class="panel-head"><h2>温度趋势</h2><div class="legend"><span>原始温度</span><span>平滑温度</span><span>趋势补偿值</span></div></div><canvas id="temperatureChart" class="chart" aria-label="温度趋势图"></canvas><div class="panel-note">显示本页面最近 180 次采样；刷新页面后重新累计。</div></article>
        <article class="panel"><div class="panel-head"><h2>设备运行状态</h2><span class="panel-note">LIVE STATUS</span></div>
          <div class="fan-wrap"><div id="fanIcon" class="fan-icon"><span>✣</span></div><div class="fan-meta"><strong id="fanState">等待状态</strong><small id="fanSub">PWM 输出状态待确认</small></div></div>
          <div class="device-state">
            <div class="state-row"><span>温度传感器</span><b id="sensorState">等待采样</b></div>
            <div class="state-row"><span>当前策略</span><b id="strategyState">--</b></div>
            <div class="state-row"><span>实验记录</span><b id="recordState">未开始</b></div>
            <div class="state-row"><span>报警状态</span><b id="alarmState">正常</b></div>
          </div>
          <div id="notice" class="notice">正在等待 ESP32 状态数据</div>
        </article>
        <article class="panel"><div class="panel-head"><h2>转速与输出</h2><div class="legend"><span>RPM</span><span>PWM 指令</span></div></div><canvas id="outputChart" class="chart small" aria-label="转速与PWM输出趋势"></canvas><div class="panel-note">PWM 为控制指令；RPM 为测速脉冲估算值。</div></article>
        <article class="panel"><div class="panel-head"><h2>本次实验摘要</h2><span class="panel-note">RAM RECORD</span></div>
          <div class="grid metrics-two">
            <div class="mini-metric"><span>最低温度</span><b id="minT">--</b></div>
            <div class="mini-metric"><span>最高温度</span><b id="maxT">--</b></div>
            <div class="mini-metric"><span>平均 PWM</span><b id="meanOutput">--</b></div>
            <div class="mini-metric"><span>档位变化</span><b id="changes">--</b></div>
          </div>
          <div class="progress"><span id="recordProgress"></span></div><div class="panel-note" id="recordProgressText">0 / 600 samples</div>
        </article>
      </div>
    </section>

    <section class="page" id="page-control">
      <div class="page-head"><div><div class="eyebrow">REMOTE OPERATION / 02</div><h1>设备控制</h1><p class="subhead">通过本地 Wi-Fi 向 ESP32 发送控制指令</p></div><div class="updated">控制链路 <b id="controlLink">等待连接</b></div></div>
      <div class="grid control-grid">
        <article class="panel">
          <div class="panel-head"><h2>运行模式</h2><span class="panel-note">CONTROL MODE</span></div>
          <form action="/control" method="post" target="result">
            <div class="btn-row"><button class="btn" id="m0" name="value" value="manual">手动基线</button><button class="btn" id="m1" name="value" value="threshold">阈值温控</button><button class="btn" id="m2" name="value" value="trend">趋势补偿</button></div>
            <div class="control-group"><div class="control-label">手动档位 · 选择后自动切换至手动模式</div><div class="btn-row"><button class="btn" id="s1" name="value" value="1">低档 · 约 30%</button><button class="btn" id="s2" name="value" value="2">中档 · 约 60%</button><button class="btn" id="s3" name="value" value="3">高档 · 100%</button></div></div>
            <div class="control-group"><div class="control-label">设备操作</div><div class="btn-row"><button class="btn" name="value" value="indicator_test">声光自检</button><button class="btn danger" id="s0" name="value" value="0">0% 输出 / 停止记录</button></div></div>
          </form>
          <p class="control-tip">记录期间策略和档位修改会被锁定；0% 输出仍可用。K1 实体按键和红外遥控保持独立工作。</p>
        </article>
        <article class="panel">
          <div class="panel-head"><h2>实时执行状态</h2><span class="panel-note">ACTUATOR</span></div>
          <div class="fan-wrap"><div id="controlFanIcon" class="fan-icon"><span>✣</span></div><div class="fan-meta"><strong id="controlFanState">--</strong><small id="controlFanDetail">等待设备状态</small></div></div>
          <div class="grid metrics-two">
            <div class="mini-metric"><span>当前 PWM 指令</span><b id="controlPwm">--</b></div>
            <div class="mini-metric"><span>估算转速</span><b id="controlRpm">--</b></div>
          </div>
          <div id="controlNotice" class="notice">控制指令仅在设备在线时可发送</div>
        </article>
      </div>
      <article class="panel" style="margin-top:14px"><div class="panel-head"><h2>控制安全说明</h2></div><p class="subhead">本平台当前采用 ESP32 SoftAP 本地控制，不依赖公网。浏览器断开不会停止设备端自动温控、实体按键或红外控制。网页离线时网页指令无法送达；紧急情况下可使用遥控器 0 键或切断风扇 12V 电源。0% PWM 指令不保证风扇机械停转。</p></article>
    </section>

    <section class="page" id="page-experiments">
      <div class="page-head"><div><div class="eyebrow">DATA MANAGEMENT / 03</div><h1>实验数据中心</h1><p class="subhead">查询设备 RAM 中的采样记录，并导出 CSV 供进一步分析</p></div><div class="updated">存储方式 <b>RAM · 最多 600 条</b></div></div>
      <article class="panel">
        <div class="panel-head"><h2>采样记录</h2><span id="tableSummary" class="panel-note">等待读取数据</span></div>
        <div class="table-tools">
          <div class="tool-left"><label class="control-label" for="recordFilter">记录筛选</label><select id="recordFilter"><option value="all">全部记录</option><option value="alarm">报警样本</option><option value="invalid">传感器无效</option><option value="hot">高温样本（≥40°C）</option></select></div>
          <div class="tool-right"><button type="button" class="btn" id="refreshRecords">刷新数据</button><a id="download" class="btn primary" href="/experiment.csv" aria-disabled="true">导出 CSV</a></div>
        </div>
        <div class="table-wrap"><table><thead><tr><th>序号</th><th>时间 / ms</th><th>原始温度</th><th>平滑温度</th><th>变化率</th><th>补偿值</th><th>模式</th><th>目标档位</th><th>PWM</th><th>RPM</th><th>状态</th></tr></thead><tbody id="recordsBody"><tr><td colspan="11" class="table-empty">暂无记录，请先开始实验</td></tr></tbody></table></div>
        <div class="pagination"><span id="pageInfo">第 0 / 0 页</span><div class="pager"><button type="button" class="btn" id="prevPage">上一页</button><button type="button" class="btn" id="nextPage">下一页</button></div></div>
      </article>
      <article class="panel" style="margin-top:14px">
        <div class="panel-head"><h2>实验记录管理</h2><span class="panel-note">EXPERIMENT SESSION</span></div>
        <div class="btn-row"><form action="/control" method="post" target="result"><button class="btn primary" name="value" value="start">开始新记录</button></form><form action="/control" method="post" target="result"><button class="btn" name="value" value="finish">结束记录</button></form><form action="/control" method="post" target="result"><button class="btn danger" name="value" value="clear">清空记录</button></form></div>
        <p class="control-tip">开始新记录会清空上次数据；最多保存 600 个样本，约 10 分钟。记录数据仅存放于 ESP32 RAM，断电会丢失。结束记录后请及时导出 CSV。</p>
      </article>
    </section>

    <section class="page" id="page-analysis">
      <div class="page-head"><div><div class="eyebrow">ALGORITHM INSIGHT / 04</div><h1>算法分析中心</h1><p class="subhead">对比阈值温控与趋势补偿策略在当前采样下的决策差异</p></div><div class="updated">实时计算 <b>双策略并行评估</b></div></div>
      <div class="grid analysis-grid">
        <article class="panel strategy-card"><div class="strategy-title"><b>阈值温控 / THRESHOLD</b><span class="panel-note">基线策略</span></div><div class="panel-note">当前建议档位</div><div class="strategy-value" id="thresholdLevel">--</div><p class="strategy-desc">根据平滑温度与 26°C、28°C、30°C 分级阈值决策，并采用 0.5°C 回差降低频繁切换。</p></article>
        <article class="panel strategy-card trend"><div class="strategy-title"><b>趋势补偿 / TREND</b><span class="panel-note">趋势策略</span></div><div class="panel-note">当前建议档位</div><div class="strategy-value" id="trendLevel">--</div><p class="strategy-desc">根据平滑温度与近似升温速率计算补偿控制值，再使用相同档位阈值形成建议。</p></article>
      </div>
      <article class="panel" style="margin-top:14px"><div class="panel-head"><h2>策略输出对比</h2><div class="legend"><span>实际目标档位</span><span>阈值建议</span><span>趋势建议</span></div></div><canvas id="strategyChart" class="chart" aria-label="控制策略对比图"></canvas><div class="panel-note" id="strategySummary">等待有效温度采样</div></article>
      <article class="panel" style="margin-top:14px"><div class="panel-head"><h2>算法口径说明</h2></div><p class="subhead">趋势补偿值是算法内部计算量，不是独立传感器测得的温度，也不是经过验证的未来温度预测。趋势补偿当前采用：平滑温度 + max(升温速率, 0) × 0.25，并将补偿量限制在 2°C 以内。两种策略同时计算，但只有当前选中的自动策略驱动风扇。</p></article>
    </section>

    <section class="page" id="page-diagnostics">
      <div class="page-head"><div><div class="eyebrow">DEVICE HEALTH / 05</div><h1>设备诊断</h1><p class="subhead">检查当前传感器、显示模块、控制器和报警状态</p></div><div class="updated">固件版本 <b id="firmware">--</b></div></div>
      <div class="grid diagnostic-grid">
        <article class="panel"><div class="panel-head"><h2>硬件状态</h2><span class="panel-note">HARDWARE</span></div>
          <div class="diagnostic-item"><span>DS18B20 温度传感器</span><b id="diagSensor">等待采样</b></div>
          <div class="diagnostic-item"><span>数码管通信 ACK</span><b id="diagDisplay">--</b></div>
          <div class="diagnostic-item"><span>风扇转速反馈</span><b id="diagTach">--</b></div>
          <div class="diagnostic-item"><span>ESP32 控制器</span><b id="diagController" class="good">在线</b></div>
        </article>
        <article class="panel"><div class="panel-head"><h2>当前报警</h2><span class="panel-note">ACTIVE ALARMS</span></div>
          <div class="diagnostic-item"><span>传感器故障</span><b id="diagSensorAlarm">未触发</b></div>
          <div class="diagnostic-item"><span>超温报警</span><b id="diagOverheat">未触发</b></div>
          <div class="diagnostic-item"><span>报警处理策略</span><b>自动模式请求全速</b></div>
          <div class="diagnostic-item"><span>报警阈值</span><b>40°C / 38°C 回差</b></div>
        </article>
      </div>
      <article class="panel" style="margin-top:14px"><div class="panel-head"><h2>运行边界与说明</h2></div><p class="subhead">报警阈值为本项目演示参数，不代表工业设备安全认证值。RPM 根据测速脉冲估算，尚未校准。当前没有持久化事件日志、云端历史数据库或远程公网控制；页面展示的是设备当前状态和 RAM 实验数据。</p></article>
    </section>
  </div>
  <footer class="footer"><span id="version">SmartFan IoT Platform · Firmware --</span> · ESP32 local edge monitoring · Offline-capable UI · 数据断电不保留</footer>
</main>
</div>
<iframe name="result" id="result" title="设备操作反馈"></iframe>
<script>
var points=[],lastMs=0,lastExperimentId=-1,lastRecordCount=-1,tablePage=0,pageSize=25,totalPages=0,lastTableRefresh=0;
function el(id){return document.getElementById(id);}
function fmt(v,d){return typeof v==="number"&&isFinite(v)?v.toFixed(d===undefined?2:d):"--";}
function modeName(m){return ["手动基线","阈值温控","趋势补偿"][m]||"未知";}
function levelName(v){return ["停止","低档","中档","高档"][v]||"未知";}
function setText(id,value){el(id).textContent=value;}
function draw(id,keys,fixed){
 var c=el(id);if(!c)return;var ctx=c.getContext("2d");c.width=900;c.height=220;
 var low=Infinity,high=-Infinity;
 points.forEach(function(p){keys.forEach(function(k){if(typeof p[k]==="number"&&isFinite(p[k])){low=Math.min(low,p[k]);high=Math.max(high,p[k]);}});});
 if(fixed){low=0;high=3;}else if(!isFinite(low)){low=0;high=40;}else{low=Math.floor(low-1);high=Math.ceil(high+1);if(high-low<2)high=low+2;}
 ctx.font="12px sans-serif";ctx.fillStyle="#8195ad";
 for(var j=0;j<=4;j++){var y=16+j*43;ctx.fillText((high-(high-low)*j/4).toFixed(fixed?0:1),4,y+4);ctx.strokeStyle="#26364d";ctx.beginPath();ctx.moveTo(48,y);ctx.lineTo(890,y);ctx.stroke();}
 if(points.length<2)return;
 var first=points[0].ms,last=points[points.length-1].ms;
 var colors=["#77aaff","#55d6d0","#f2bd68"];
 keys.forEach(function(k,i){ctx.beginPath();ctx.strokeStyle=colors[i%3];ctx.lineWidth=2;var connected=false;
  points.forEach(function(p){if(typeof p[k]!=="number"||!isFinite(p[k])){connected=false;return;}
   var x=48+(p.ms-first)/(last-first||1)*842,y=188-(p[k]-low)/(high-low||1)*172;
   if(connected)ctx.lineTo(x,y);else{ctx.moveTo(x,y);connected=true;}
  });ctx.stroke();
 });
 ctx.fillStyle="#8195ad";ctx.fillText("采样窗口 "+Math.round((last-first)/1000)+" 秒",48,214);ctx.fillText("当前",855,214);
}
function updateLevelButtons(s){
 document.querySelectorAll('button[name="value"]').forEach(function(b){
  b.classList.remove("active");
  var locked=["manual","threshold","trend","1","2","3","start","clear"].indexOf(b.value)>=0;
  b.disabled=!!s.recording&&locked;
  if(b.value==="finish")b.disabled=!s.recording;
  if(b.value==="manual"&&s.mode===0)b.classList.add("active");
  if(b.value==="threshold"&&s.mode===1)b.classList.add("active");
  if(b.value==="trend"&&s.mode===2)b.classList.add("active");
  if(b.value===String(s.level)&&["1","2","3"].indexOf(b.value)>=0)b.classList.add("active");
 });
}
function show(s){
 if(typeof s.ms==="number"&&s.ms<lastMs)points=[];
 lastMs=s.ms||lastMs;
 el("connection").textContent="设备在线";el("connection").className="status-pill online";
 setText("updatedAt",new Date().toLocaleTimeString("zh-CN",{hour12:false}));
 setText("temp",s.sampled?(s.raw===null?"采样无效":fmt(s.raw)+" °C"):"等待采样");
 setText("rpm",typeof s.rpm==="number"?s.rpm+" RPM":"-- RPM");
 setText("pwm",Math.round(s.pwm*100/255)+" %");
 setText("modeText",modeName(s.mode));
 setText("filtered","平滑温度 "+fmt(s.filtered)+" °C");
 setText("rate",s.rate===null?"升温速率：窗口积累中":"升温速率："+fmt(s.rate)+" °C/min");
 setText("levelText","目标档位："+levelName(s.level));
 setText("fanState",s.pwm>0?"运行中":"当前输出为 0%");
 setText("fanSub","PWM "+s.pwm+"/255 · "+modeName(s.mode));
 setText("sensorState",s.raw===null?"采样无效":"采样正常");
 setText("strategyState",modeName(s.mode));
 setText("recordState",s.recording?"记录中 · "+s.count+"/600":(s.count>0?"已结束 · "+s.count+" 条":"未开始"));
 var alarmText=[];if(s.alarms&1)alarmText.push("传感器故障");if(s.alarms&2)alarmText.push("超温报警");
 setText("alarmState",alarmText.length?alarmText.join(" / "):"正常");
 el("alarmState").className=alarmText.length?"bad":"good";
 var notice=alarmText.length?alarmText.join(" / ")+(s.mode===0?"；手动模式，请检查设备":"；自动模式已请求全速"):(!s.sampled?"等待首次采样":s.raw===null?"采样暂时无效；自动模式保持之前输出":"设备运行正常");
 setText("notice",notice);el("notice").className="notice"+(alarmText.length?" bad":(s.raw===null?" warn":""));
 var running=s.pwm>0;el("fanIcon").className="fan-icon"+(running?" running":"");el("controlFanIcon").className="fan-icon"+(running?" running":"");
 setText("controlFanState",running?"运行中":"停止指令 / 低输出");setText("controlFanDetail",modeName(s.mode)+" · "+levelName(s.level));
 setText("controlPwm",Math.round(s.pwm*100/255)+"%");setText("controlRpm",typeof s.rpm==="number"?s.rpm+" RPM":"--");
 setText("controlLink","在线");setText("controlNotice",alarmText.length?alarmText.join(" / "):"设备在线，可发送控制指令");
 setText("minT",s.minT===null?"--":fmt(s.minT)+" °C");setText("maxT",s.maxT===null?"--":fmt(s.maxT)+" °C");
 setText("meanOutput",s.meanOutput===null?"--":fmt(s.meanOutput,1)+"%");setText("changes",s.count?s.changes+" 次":"--");
 el("recordProgress").style.width=(s.count/600*100)+"%";setText("recordProgressText",s.count+" / 600 samples");
 setText("thresholdLevel",levelName(s.baseline));setText("trendLevel",levelName(s.trend));
 setText("strategySummary","当前实际模式："+modeName(s.mode)+"；阈值建议："+levelName(s.baseline)+"；趋势建议："+levelName(s.trend)+"。建议档位不等于实际执行档位。");
 setText("firmware",s.firmware||"--");setText("version","SmartFan IoT Platform · Firmware "+(s.firmware||"--"));
 setText("diagSensor",s.raw===null?"无效 / 待检查":"正常");el("diagSensor").className=s.raw===null?"bad":"good";
 setText("diagDisplay",s.displayAck?"已应答":"未应答");el("diagDisplay").className=s.displayAck?"good":"warn";
 setText("diagTach",typeof s.rpm==="number"?s.rpm+" RPM（估算）":"暂无有效脉冲");el("diagTach").className=typeof s.rpm==="number"?"good":"warn";
 setText("diagSensorAlarm",(s.alarms&1)?"已触发":"未触发");el("diagSensorAlarm").className=(s.alarms&1)?"bad":"good";
 setText("diagOverheat",(s.alarms&2)?"已触发":"未触发");el("diagOverheat").className=(s.alarms&2)?"bad":"good";
 setText("controlLink","在线");
 updateLevelButtons(s);
 if(s.ms!==undefined){var p=Object.assign({},s);p.pwmPct=s.pwm*100/255;points.push(p);if(points.length>180)points.shift();}
 draw("temperatureChart",["raw","filtered","predicted"],false);
 draw("outputChart",["rpm","pwmPct"],false);
 draw("strategyChart",["level","baseline","trend"],true);
 if(s.experimentId!==lastExperimentId){lastExperimentId=s.experimentId;tablePage=0;lastRecordCount=-1;}
 if(s.count!==lastRecordCount){lastRecordCount=s.count;if(el("page-experiments").classList.contains("active"))loadRecords();}
 el("download").setAttribute("aria-disabled",s.recording||!s.count?"true":"false");
}
function fail(){
 el("connection").textContent="连接中断";el("connection").className="status-pill offline";
 setText("notice","设备连接中断，页面数据已过时。请检查 ESP32-SmartFan Wi-Fi；离线期间网页控制指令无法送达。");
 el("notice").className="notice warn";setText("controlLink","离线");setText("controlNotice","连接中断，无法发送网页指令");
 document.querySelectorAll('button[name="value"]').forEach(function(b){b.disabled=b.value!=="0";});
 el("download").setAttribute("aria-disabled","true");
}
function poll(){
 var x=new XMLHttpRequest();x.open("GET","/status?t="+Date.now());x.timeout=4000;
 x.onload=function(){try{if(x.status!==200)throw Error("status");show(JSON.parse(x.responseText));}catch(e){fail();}};
 x.onerror=fail;x.ontimeout=fail;x.onloadend=function(){setTimeout(poll,1000);};x.send();
}
function loadRecords(){
 var filter=el("recordFilter").value,offset=tablePage*pageSize;
 var x=new XMLHttpRequest();x.open("GET","/records?offset="+offset+"&limit="+pageSize+"&filter="+encodeURIComponent(filter)+"&t="+Date.now());x.timeout=5000;
 x.onload=function(){
  if(x.status!==200){setText("tableSummary","读取失败："+x.status);return;}
  try{
   var d=JSON.parse(x.responseText),items=d.items||[],total=d.total||0;
   totalPages=Math.max(1,Math.ceil(total/pageSize));
   setText("tableSummary","共 "+total+" 条匹配记录 · 当前页 "+(items.length?tablePage+1:0)+" / "+(total?totalPages:0));
   setText("pageInfo","第 "+(total?tablePage+1:0)+" / "+(total?totalPages:0)+" 页");
   el("prevPage").disabled=tablePage<=0;el("nextPage").disabled=tablePage+1>=totalPages;
   if(!items.length){el("recordsBody").innerHTML='<tr><td colspan="11" class="table-empty">当前筛选条件下暂无记录</td></tr>';return;}
   el("recordsBody").innerHTML=items.map(function(r){
    var status=r.alarms?"报警":(!r.valid?"传感器无效":"正常");
    var cls=r.alarms||!r.valid?"bad":"good";
    return "<tr><td>"+r.index+"</td><td>"+r.elapsedMs+"</td><td>"+fmt(r.raw)+"</td><td>"+fmt(r.filtered)+"</td><td>"+fmt(r.rate,2)+"</td><td>"+fmt(r.predicted)+"</td><td>"+r.mode+"</td><td>"+levelName(r.target)+"</td><td>"+Math.round(r.pwm*100/255)+"%</td><td>"+(r.rpm===null?"--":r.rpm)+"</td><td class='"+cls+"'>"+status+"</td></tr>";
   }).join("");
  }catch(e){setText("tableSummary","数据解析失败");}
 };
 x.onerror=function(){setText("tableSummary","设备连接中断");};x.ontimeout=function(){setText("tableSummary","读取超时");};x.send();
}
document.querySelectorAll(".nav-btn").forEach(function(b){b.addEventListener("click",function(){
 var page=b.getAttribute("data-page");
 document.querySelectorAll(".nav-btn").forEach(function(n){n.classList.toggle("active",n===b);});
 document.querySelectorAll(".page").forEach(function(p){p.classList.toggle("active",p.id==="page-"+page);});
 setText("crumbTitle",{"overview":"设备总览","control":"设备控制","experiments":"实验数据","analysis":"算法分析","diagnostics":"设备诊断"}[page]||"设备总览");
 if(page==="experiments")loadRecords();
 if(page==="overview"||page==="analysis"){draw("temperatureChart",["raw","filtered","predicted"],false);draw("strategyChart",["level","baseline","trend"],true);}
 });});
el("recordFilter").addEventListener("change",function(){tablePage=0;loadRecords();});
el("refreshRecords").addEventListener("click",loadRecords);
el("prevPage").addEventListener("click",function(){if(tablePage>0){tablePage--;loadRecords();}});
el("nextPage").addEventListener("click",function(){if(tablePage+1<totalPages){tablePage++;loadRecords();}});
document.querySelectorAll('button[name="value"]').forEach(function(b){b.addEventListener("click",function(e){
 if((b.value==="start"||b.value==="clear")&&!confirm("此操作会清空当前 RAM 实验记录，是否继续？"))e.preventDefault();
 });});
el("download").addEventListener("click",function(e){if(this.getAttribute("aria-disabled")==="true")e.preventDefault();});
poll();
</script>
</body>
</html>
)SFHTML";
