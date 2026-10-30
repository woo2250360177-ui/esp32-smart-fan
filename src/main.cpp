#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <WebServer.h>
#include <math.h>
#include <stdint.h>
struct TemperatureControl {
  float history[11] = {};
  uint32_t times[11] = {};
  int count=0, next=0, thresholdLevel=0, trendLevel=0;
  bool valid=false, ready=false;
  float filtered=0, rate=0, predicted=0;
  void reset() {
    count=next=thresholdLevel=trendLevel=0;
    valid=ready=false; filtered=rate=predicted=0;
  }
  static int level(float t, int old) {
    const float limits[]={26,28,30};
    int n=old;
    while(n<3 && t>=limits[n]) ++n;
    while(n>0 && t<limits[n-1]-0.5f) --n;
    return n;
  }
  void sample(float raw,uint32_t now,bool ok) {
    if(!ok || !isfinite(raw) || raw < -55 || raw >125) {reset();return;}
    if(!valid) filtered=raw; else filtered+=0.25f*(raw-filtered);
    valid=true; history[next]=filtered; times[next]=now;
    next=(next+1)%11; if(count<11) ++count;
    rate=0; ready=false;
    if(count==11) {
      uint32_t dt=now-times[next];
      if(dt>=10000) {rate=(filtered-history[next])*60000.0f/dt;ready=true;}
    }
    float extra=ready && rate>0.2f ? rate*0.25f : 0;
    if(extra>2)extra=2;
    predicted=filtered+extra;
    thresholdLevel=level(filtered,thresholdLevel);
    trendLevel=level(predicted,trendLevel);
  }
};


// Wiring: GPIO15 -> IN1, GPIO27 -> K1, GPIO26 -> DS.
const int MOTOR=15, BUTTON=27, DS=26, CHANNEL=0;
const int DUTY[]={0,128,192,255};
const char* MODE_NAMES[]={"MANUAL","THRESHOLD","TREND"};
enum Mode {MANUAL, THRESHOLD, TREND};
Mode mode=MANUAL, lastAuto=THRESHOLD;
// Alarm thresholds are demonstration settings, not equipment safety limits.
// Automatic modes use full speed on a confirmed fault. Manual control stays explicit.
const float ALARM_HIGH_C=40.0f, ALARM_CLEAR_C=38.0f;
bool sensorAlarm=false, overheatAlarm=false, sampled=false;
uint8_t badSamples=0, goodSamples=0, hotSamples=0, coolSamples=0;
void updateAlarms(float raw, bool valid) {
  sampled=true;
  if(!valid) {
    goodSamples=hotSamples=coolSamples=0;
    if(badSamples<3) ++badSamples;
    if(badSamples>=3)sensorAlarm=true;
    return;
  }
  badSamples=0;
  if(goodSamples<3) ++goodSamples;
  if(goodSamples>=3)sensorAlarm=false;
  if(raw>=ALARM_HIGH_C) {
    coolSamples=0;
    if(hotSamples<3) ++hotSamples;
    if(hotSamples>=3)overheatAlarm=true;
  } else if(raw<=ALARM_CLEAR_C) {
    hotSamples=0;
    if(coolSamples<5) ++coolSamples;
    if(coolSamples>=5)overheatAlarm=false;
  } else {hotSamples=coolSamples=0;}
}
uint8_t alarmBits(){return (sensorAlarm?1:0)|(overheatAlarm?2:0);}

OneWire wire(DS);
DallasTemperature sensors(&wire);
WebServer server(80);
TemperatureControl ctl;
float rawTemperature=0;
int actualLevel=0, manualLevel=0, appliedPwm=0;
bool boosting=false;
uint32_t boostAt=0, conversionAt=0, rampAt=0;
int lastReading=HIGH, stableButton=HIGH;
uint32_t changedAt=0, pressedAt=0;
bool longHandled=false;

// RAM-only recording; stops at 600 samples. Export before power off.
struct Record {
  uint32_t ms;
  float raw, filtered, rate, predicted;
  uint8_t mode, actual, pwm, baseline, trend, valid, ready, alarms;
};
const int CAPACITY=600;
Record records[CAPACITY];
int recordCount=0;
bool recording=false;
uint32_t recordAt=0;

void writePwm(int duty) {appliedPwm=duty;ledcWrite(CHANNEL,duty);}
void setSpeed(int level) {
  if(level<0 || level>3 || level==actualLevel)return;
  bool stopped=actualLevel==0;
  actualLevel=level;
  if(!level){boosting=false;writePwm(0);}
  else if(stopped){boosting=true;boostAt=millis();writePwm(255);}
  // Running changes are handled by the nonblocking ramp.
  Serial.printf("Speed: %d\n",actualLevel);
}
void handleMotor(){
  uint32_t now=millis();
  if(boosting){
    if(now-boostAt<300)return;
    boosting=false;writePwm(DUTY[actualLevel]);rampAt=now;return;
  }
  if(now-rampAt<20)return;
  rampAt=now;
  int target=DUTY[actualLevel], delta=target-appliedPwm;
  if(delta>4)delta=4;if(delta< -4)delta=-4;
  if(delta)writePwm(appliedPwm+delta);
}
void applyControl() {
  if(mode==MANUAL)return;
  if(alarmBits()){setSpeed(3);boosting=false;writePwm(255);}
  else if(!ctl.valid)return; // Startup holds stop; transient failure holds previous output.
  else setSpeed(mode==THRESHOLD ? ctl.thresholdLevel : ctl.trendLevel);
}
void selectMode(Mode selected) {
  recording=false; // Physical mode/level changes end the current experiment.
  if(selected==MANUAL && mode!=MANUAL)manualLevel=actualLevel;
  mode=selected;
  if(mode==MANUAL)setSpeed(manualLevel);
  else {lastAuto=mode;applyControl();}
}
void toggleMode(){selectMode(mode==MANUAL ? lastAuto : MANUAL);}
void shortPress(){
  selectMode(MANUAL);manualLevel=(manualLevel+1)%4;setSpeed(manualLevel);
}
void handleButton(){
  uint32_t now=millis();int reading=digitalRead(BUTTON);
  if(reading!=lastReading){lastReading=reading;changedAt=now;}
  if(now-changedAt>=30 && reading!=stableButton){
    stableButton=reading;
    if(reading==LOW){pressedAt=now;longHandled=false;}
    else if(!longHandled){
      if(now-pressedAt>=1000)toggleMode();else shortPress();
    }
  }
  if(stableButton==LOW && reading==LOW && !longHandled && now-pressedAt>=1000){
    longHandled=true;toggleMode();
  }
}
void handleTemperature(){
  if(millis()-conversionAt<1000)return;
  rawTemperature=sensors.getTempCByIndex(0);
  uint32_t now=millis();
  ctl.sample(rawTemperature,now,rawTemperature!=DEVICE_DISCONNECTED_C);
  uint8_t oldAlarms=alarmBits();
  updateAlarms(rawTemperature,ctl.valid);
  if(oldAlarms!=alarmBits())Serial.printf("Alarm changed: %u -> %u\n",oldAlarms,alarmBits());
  applyControl();
  if(recording && recordCount<CAPACITY){
    records[recordCount++]={now-recordAt,rawTemperature,ctl.filtered,ctl.rate,
      ctl.predicted,(uint8_t)mode,(uint8_t)actualLevel,(uint8_t)appliedPwm,
      (uint8_t)ctl.thresholdLevel,(uint8_t)ctl.trendLevel,
      (uint8_t)ctl.valid,(uint8_t)ctl.ready,alarmBits()};
    if(recordCount==CAPACITY)recording=false;
  }
  Serial.printf("[%s] T=%.2f filtered=%.2f rate=%.2f valid=%d level=%d PWM=%d\n",
    MODE_NAMES[mode],rawTemperature,ctl.filtered,ctl.rate,ctl.valid,actualLevel,appliedPwm);
  sensors.requestTemperatures();conversionAt=millis();
}
String numberOrNull(float v){return ctl.valid ? String(v,2) : "null";}
void sendStatus(){
  String s;s.reserve(400);
  s="{\"ms\":"+String(millis())+",\"mode\":"+String((int)mode);
  s+=",\"raw\":"+numberOrNull(rawTemperature);
  s+=",\"filtered\":"+numberOrNull(ctl.filtered);
  s+=",\"predicted\":"+numberOrNull(ctl.predicted);
  s+=",\"rate\":"+(ctl.ready ? String(ctl.rate,2) : String("null"));
  s+=",\"level\":"+String(actualLevel)+",\"pwm\":"+String(appliedPwm);
  s+=",\"baseline\":"+String(ctl.thresholdLevel);
  s+=",\"trend\":"+String(ctl.trendLevel);
  s+=",\"recording\":"+String(recording ? "true" : "false");
  s+=",\"alarms\":"+String(alarmBits());
  s+=",\"sampled\":"+String(sampled?"true":"false");
  s+=",\"rpm\":null"; // No tach hardware configured: never invent an RPM value.
  int validCount=0, changes=0;float lo=INFINITY,hi=-INFINITY;uint32_t pwmSum=0;
  for(int i=0;i<recordCount;i++){
    const Record& r=records[i];pwmSum+=r.pwm;
    if(r.valid){validCount++;if(r.raw<lo)lo=r.raw;if(r.raw>hi)hi=r.raw;}
    if(i && r.actual!=records[i-1].actual)changes++;
  }
  s+=",\"minT\":"+(validCount?String(lo,2):String("null"));
  s+=",\"maxT\":"+(validCount?String(hi,2):String("null"));
  s+=",\"meanOutput\":"+(recordCount?String(pwmSum*100.0f/(255*recordCount),1):String("null"));
  s+=",\"validCount\":"+String(validCount)+",\"changes\":"+String(changes);
  s+=",\"count\":"+String(recordCount)+"}";
  server.sendHeader("Cache-Control","no-store");
  server.send(200,"application/json",s);
}
void downloadCsv(){
  if(recording){server.send(409,"text/plain; charset=utf-8","请先结束记录再下载");return;}
  server.sendHeader("Cache-Control","no-store");
  server.sendHeader("Content-Disposition","attachment; filename=fan_experiment.csv");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200,"text/csv; charset=utf-8","");
  server.sendContent("elapsed_ms,raw_C,filtered_C,rate_C_per_min,predicted_C,mode,actual_level,pwm_command,threshold_suggestion,trend_suggestion,sensor_valid,trend_ready,alarm_bits\r\n");
  String chunk;chunk.reserve(1800);
  for(int i=0;i<recordCount;i++){
    const Record& r=records[i];
    chunk+=String(r.ms)+",";
    if(r.valid)chunk+=String(r.raw,3);chunk+=",";
    if(r.valid)chunk+=String(r.filtered,3);chunk+=",";
    if(r.ready)chunk+=String(r.rate,3);chunk+=",";
    if(r.valid)chunk+=String(r.predicted,3);
    chunk+=","+String(MODE_NAMES[r.mode])+","+String(r.actual)+","+String(r.pwm);
    chunk+=","+String(r.baseline)+","+String(r.trend)+","+String(r.valid);
    chunk+=","+String(r.ready)+","+String(r.alarms)+"\r\n";
    if(chunk.length()>1400){server.sendContent(chunk);chunk="";yield();}
  }
  if(chunk.length())server.sendContent(chunk);
  server.sendContent("");
}

const char PAGE[] PROGMEM=R"HTML(
<!doctype html><html lang="zh-CN"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>自适应散热与状态监测</title>
<style>
body{font-family:system-ui,sans-serif;background:#edf2f7;color:#1e293b;max-width:720px;margin:auto;padding:20px}
section{background:white;border-radius:18px;padding:20px;margin:16px 0}h1{font-size:24px}h2{font-size:18px}
.grid{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}
button,a.download{border:0;border-radius:10px;padding:14px 8px;background:#e8eef6;font-size:16px;cursor:pointer;color:#203047}
button.active{background:#2563eb;color:white}.stop{color:#b91c1c}#temp{font-size:42px;font-weight:600}
canvas{width:100%;height:200px;background:#f8fafc;border-radius:8px}small,.note{color:#64748b;font-size:13px}#notice{color:#b45309}

:root{color-scheme:dark}body{background:#0b1220;color:#e5edf7;max-width:960px;padding:24px;font-family:system-ui,sans-serif}
section{background:#131f30;border:1px solid #29384c;border-radius:20px;padding:24px;box-shadow:0 12px 32px #0002}
h1{font-size:26px;letter-spacing:1px}h2{color:#d9e6f4}button,a.download{background:#25354b;color:#dce9f8;border:1px solid #364b66;transition:background .2s}
button.active{background:#27c7a2;color:#052c25;border-color:#27c7a2}button:disabled{opacity:.35;cursor:not-allowed}.stop{color:#ffaaa6;border-color:#a04848}
#temp{font-size:54px;color:#62e3c3;font-variant-numeric:tabular-nums}small,.note{color:#a6b7cd;line-height:1.8}#notice{color:#ffd28c}
canvas{background:#101a29}a.download{display:inline-block;text-decoration:none}.metrics{display:grid;grid-template-columns:repeat(4,1fr);gap:12px}.metric{padding:14px;background:#0c1726;border-radius:12px}.metric b{display:block;font-size:23px;color:#62e3c3;margin-top:8px}.metric span{font-size:12px;color:#a6b7cd}
@media(max-width:600px){body{padding:12px}section{padding:16px}.metrics{grid-template-columns:repeat(2,1fr)}button{font-size:14px}}
</style></head><body><h1>自适应散热与状态监测 v6</h1>
<section><div id="temp">-- ℃</div><p id="state">正在连接</p><p id="detail"></p><p id="rpm">实际转速：未接入测速硬件</p><p id="notice"></p>
<form action="/control" method="post" target="result"><div class="grid">
<button id="m0" name="value" value="manual">固定风速 / 手动</button><button id="m1" name="value" value="threshold">阈值策略</button><button id="m2" name="value" value="trend">趋势策略</button>
<button id="s1" name="value" value="1">低速</button><button id="s2" name="value" value="2">中速</button><button id="s3" name="value" value="3">高速</button>
<button id="s0" class="stop" name="value" value="0">停止电机</button></div></form>
<p class="note">K1 短按换档，长按切换手动与上次自动策略。手机断开不停止电机。</p></section>
<section><h2>温度与预测值（℃）</h2><canvas id="temperature"></canvas><small>蓝：原始温度　绿：平滑温度　橙：趋势预测值（不是实测值）</small>
<h2>档位比较（0–3）</h2><canvas id="levels"></canvas><small>蓝：实际档位　绿：阈值建议　橙：趋势建议。档位和 PWM 都不是实测 RPM。</small>
<p class="note">曲线保留网页打开后的最近约 3 分钟；刷新页面会清空曲线。</p></section>
<section><h2>本次实验统计</h2><div class="metrics">
<div class="metric"><span>最低温度</span><b id="minT">—</b></div>
<div class="metric"><span>最高温度</span><b id="maxT">—</b></div>
<div class="metric"><span>平均控制输出</span><b id="meanOutput">—</b></div>
<div class="metric"><span>目标档位变化</span><b id="changes">—</b></div></div>
<p class="note">统计来自设备本次记录，刷新页面不丢失；平均输出按采样点计算，不代表功耗或实际转速。</p></section>
<section><h2>实验记录</h2><p id="record">尚未读取</p>
<form action="/control" method="post" target="result"><div class="grid">
<button name="value" value="start">开始记录</button><button name="value" value="finish">结束记录</button><button name="value" value="clear">清空记录</button></div></form>
<p><a class="download" href="/experiment.csv">下载 CSV</a></p>
<p class="note">开始会清空上次记录，只记录、不自动换策略。最多 600 个样本（约 10 分钟），满后停止记录但电机继续运行。结束记录不会停止电机。断电丢失数据，请先导出。下载期间控制响应可能短暂延迟。</p>
<p class="note">两种策略使用同一段平滑温度；只有选中的策略驱动电机。三种策略须在相同条件下分次记录；固定风速用手动档位保持不变。这是算法响应对比，不能据此证明降温更快或节能。</p>
<iframe name="result" title="操作结果" style="width:100%;height:45px;border:0"></iframe></section>
<script>
var points=[],lastMs=0;
function draw(id,keys,fixed){
 var c=document.getElementById(id),ctx=c.getContext('2d');
 c.width=640;c.height=200;var low=Infinity,high=-Infinity;
 points.forEach(function(p){keys.forEach(function(k){if(p[k]!==null){low=Math.min(low,p[k]);high=Math.max(high,p[k]);}})});
 if(fixed){low=0;high=3;}else if(!isFinite(low)){low=20;high=30;}else{low=Math.floor(low-0.5);high=Math.ceil(high+0.5);}
 ctx.fillStyle='#64748b';ctx.font='12px sans-serif';
 for(var j=0;j<=3;j++){var y=15+j*50;ctx.fillText((high-(high-low)*j/3).toFixed(1),3,y+4);ctx.strokeStyle='#dce4ee';ctx.beginPath();ctx.moveTo(40,y);ctx.lineTo(630,y);ctx.stroke();}
 if(points.length<2)return;
 var first=points[0].ms,last=points[points.length-1].ms,colors=['#2563eb','#059669','#d97706'];
 keys.forEach(function(k,index){ctx.beginPath();ctx.strokeStyle=colors[index];ctx.lineWidth=2;var connected=false;
 points.forEach(function(p){if(p[k]===null){connected=false;return;}var x=40+(p.ms-first)/(last-first||1)*590,y=165-(p[k]-low)/(high-low||1)*150;if(!connected){ctx.moveTo(x,y);connected=true;}else ctx.lineTo(x,y);});ctx.stroke();});
 ctx.fillText('距现在 '+Math.round((last-first)/1000)+' 秒',40,190);ctx.fillText('现在',598,190);
}
function show(s){
 ['minT','maxT'].forEach(function(k){document.getElementById(k).textContent=s[k]===null?'—':s[k].toFixed(2)+' °C';});
 document.getElementById('meanOutput').textContent=s.meanOutput===null?'—':s.meanOutput.toFixed(1)+'%';
 document.getElementById('changes').textContent=s.count?s.changes+' 次':'—';
 document.querySelectorAll('button').forEach(function(b){b.disabled=s.recording && ['manual','threshold','trend','1','2','3','start','clear'].indexOf(b.value)>=0;});

 if(s.ms<lastMs)points=[];lastMs=s.ms;
 document.getElementById('temp').textContent=!s.sampled?'等待首次测温':s.raw===null?'测温无效':s.raw.toFixed(2)+' ℃';
 document.getElementById('state').textContent=['固定风速 / 手动','分档温控','升温趋势'][s.mode]+' · '+['停止','低速','中速','高速'][s.level]+' · PWM '+s.pwm+'/255';
 document.getElementById('detail').textContent='升温速率：'+(s.rate===null?'采样中（约需 10 秒）':s.rate.toFixed(2)+' ℃/分钟');
 var alarms=[];
 if(s.alarms&1)alarms.push('传感器故障');
 if(s.alarms&2)alarms.push('持续高温');
 document.getElementById('notice').textContent=alarms.length?alarms.join('；')+(s.mode===0?'；当前为手动，请检查设备':'；自动模式已请求全速散热'):!s.sampled?'等待首次测温':s.raw===null?'测温暂时无效，自动模式保持上一输出':'运行正常';
 document.getElementById('rpm').textContent=s.rpm===null?'实际转速：未接入测速硬件':('实际转速：'+s.rpm+' RPM');
 document.getElementById('record').textContent=(s.recording?'正在记录':'记录已停止')+' · '+s.count+'/600 个样本';
 document.querySelectorAll('button').forEach(function(b){b.classList.remove('active');});
 document.getElementById('m'+s.mode).classList.add('active');document.getElementById('s'+s.level).classList.add('active');
 if(s.raw===null){s.baseline=null;s.trend=null;}points.push(s);if(points.length>180)points.shift();
 draw('temperature',['raw','filtered','predicted'],false);draw('levels',['level','baseline','trend'],true);
}
function poll(){var x=new XMLHttpRequest();x.open('GET','/status?t='+Date.now());x.timeout=4000;
 x.onload=function(){try{if(x.status!==200)throw Error();show(JSON.parse(x.responseText));}catch(e){fail();}};
 x.onerror=fail;x.ontimeout=fail;x.onloadend=function(){setTimeout(poll,1000);};x.send();}
function fail(){document.querySelectorAll('button').forEach(function(b){b.disabled=b.value!=='0';});document.getElementById('notice').textContent='连接中断，显示可能已过时';}
document.querySelectorAll('button').forEach(function(b){b.addEventListener('click',function(e){
 if((b.value==='start'||b.value==='clear')&&!confirm('这会清除上次记录，确定继续？'))e.preventDefault();
});});
poll();
</script></body></html>
)HTML";

void handleControl(){
  String v=server.arg("value"),message="操作成功";
  Serial.println("Web command: "+v);
  // Keep each experiment in one mode. An explicit stop always remains available.
  if(recording && (v=="manual" || v=="threshold" || v=="trend" ||
                    v=="1" || v=="2" || v=="3")) {
    server.send(409,"text/plain; charset=utf-8","请先结束记录，再切换策略或档位");return;
  }
  if(v=="0")recording=false;
  if(v=="manual")selectMode(MANUAL);
  else if(v=="threshold")selectMode(THRESHOLD);
  else if(v=="trend")selectMode(TREND);
  else if(v=="0" || v=="1" || v=="2" || v=="3"){
    mode=MANUAL;manualLevel=v.toInt();setSpeed(manualLevel);
  }else if(v=="start"){
    if(recording)message="已在记录，未重新开始";
    else{recordCount=0;recordAt=millis();recording=true;message="新实验已开始，上次记录已清空";}
  }else if(v=="finish"){recording=false;message="记录已结束，电机模式不变";}
  else if(v=="clear"){
    if(recording){server.send(409,"text/plain; charset=utf-8","请先结束记录再清空");return;}
    recordCount=0;message="记录已清空";
  }
  else{server.send(400,"text/plain; charset=utf-8","无效指令");return;}
  server.sendHeader("Cache-Control","no-store");
  server.send(200,"text/plain; charset=utf-8",message);
}
void setup(){
  Serial.begin(115200);pinMode(BUTTON,INPUT_PULLUP);
  ledcSetup(CHANNEL,1000,8);ledcAttachPin(MOTOR,CHANNEL);writePwm(0);
  sensors.begin();sensors.setResolution(12);sensors.setWaitForConversion(false);
  sensors.requestTemperatures();conversionAt=millis();
  WiFi.mode(WIFI_AP);
  if(!WiFi.softAP("ESP32-SmartFan","Fan123456")){
    Serial.println("AP failed; local control available.");return;
  }
  server.on("/",HTTP_GET,[](){
    server.sendHeader("Cache-Control","no-store");
    server.send_P(200,"text/html; charset=utf-8",PAGE);
  });
  server.on("/status",HTTP_GET,sendStatus);
  server.on("/control",HTTP_POST,handleControl);
  server.on("/experiment.csv",HTTP_GET,downloadCsv);
  server.onNotFound([](){server.send(404,"text/plain","Not found");});
  server.begin();
  Serial.print("Thermal monitoring v6: http://");Serial.println(WiFi.softAPIP());
}
void loop(){
  handleButton();
  handleMotor();
  handleTemperature();server.handleClient();
}

