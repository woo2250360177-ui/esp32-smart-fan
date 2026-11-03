// TL-C12C integrated firmware. ESP32 Arduino 2.x, standalone main.cpp.
// GPIO32 -> 1k -> 2N3904 B; E -> GND; C -> PWM blue.
// GPIO33 <- tach yellow, 10k pull-up to 3V3. 12V negative shares GND.
// DS26, K1=27, IR14, LED21, BEEP25. Two pulses/rev assumed.
#include <Arduino.h>
#include <math.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <WebServer.h>
#include <stdint.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>
// Display bus timing selected by the user's physical diagnostic.
namespace TempDisplay {
constexpr uint8_t clkPin=18, dioPin=19;
constexpr uint16_t bitUs=500;
constexpr uint8_t DIGITS[10]={0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};
constexpr uint8_t DASH=0x40;
bool acknowledged=false;
char lastAck[10]="000000000";
uint32_t bootAt=0, updatedAt=0;
void releaseLine(uint8_t pin){pinMode(pin,INPUT_PULLUP);}
void pullLow(uint8_t pin){digitalWrite(pin,LOW);pinMode(pin,OUTPUT);}
void tick(){delayMicroseconds(bitUs);}
void stopBus(){pullLow(clkPin);pullLow(dioPin);tick();releaseLine(clkPin);tick();releaseLine(dioPin);tick();}
void startBus(){releaseLine(dioPin);releaseLine(clkPin);tick();pullLow(dioPin);tick();pullLow(clkPin);tick();}
bool writeByte(uint8_t value){
  for(uint8_t i=0;i<8;i++){
    pullLow(clkPin);tick();
    if(value&1)releaseLine(dioPin);else pullLow(dioPin);
    tick();releaseLine(clkPin);tick();
    pullLow(clkPin);tick();value>>=1;
  }
  releaseLine(dioPin);tick();releaseLine(clkPin);tick();
  const bool ack=digitalRead(dioPin)==LOW;
  pullLow(clkPin);tick();releaseLine(dioPin);tick();
  return ack;
}

// 9 ACK results: data command, start address, six display bytes, display-on.
bool sendFrame(const uint8_t data[6],char result[10]){
  uint8_t n=0;bool ok=true;
  startBus();bool a=writeByte(0x40);result[n++]=a?'1':'0';ok &= a;stopBus();
  startBus();a=writeByte(0xc0);result[n++]=a?'1':'0';ok &= a;
  for(uint8_t i=0;i<6;i++){a=writeByte(data[i]);result[n++]=a?'1':'0';ok &= a;}
  stopBus();startBus();a=writeByte(0x8c);result[n++]=a?'1':'0';ok &= a;stopBus();
  result[n]=0;return ok;
}

void segments(const uint8_t data[4]){
  const uint8_t frame[6]={data[0],data[1],data[2],data[3],0,0};
  acknowledged=sendFrame(frame,lastAck);
}
void encode(float temp,bool valid,uint8_t data[4]){
  for(uint8_t i=0;i<4;++i)data[i]=0;
  if(!valid || !isfinite(temp) || temp < -55 || temp > 125){
    for(uint8_t i=0;i<4;++i)data[i]=DASH;
    return;
  }
  int value=(int)lroundf(temp*10.0f);
  bool negative=value<0;
  if(negative)value=-value;
  data[3]=DIGITS[value%10]; value/=10;
  data[2]=DIGITS[value%10]|0x80; value/=10; // Decimal point before tenths.
  int pos=1;
  while(value>0 && pos>=0){data[pos--]=DIGITS[value%10];value/=10;}
  if(negative && pos>=0)data[pos]=DASH;
}
void begin(){
  releaseLine(clkPin);releaseLine(dioPin);
  delay(2000); // Power-on settling, only once at boot.
  stopBus();delay(20);
  const uint8_t all[4]={0xff,0xff,0xff,0xff};segments(all);
  bootAt=updatedAt=millis();
  Serial.printf("DISPLAY init ACK=%d bits=%s (CLK18 DIO19 delay_us=500)\n",acknowledged,lastAck);
}
void update(float temp,bool valid){
  const uint32_t now=millis();
  if(now-bootAt<1000 || now-updatedAt<1000)return;
  updatedAt=now;
  uint8_t data[4];encode(temp,valid,data);
  const bool before=acknowledged;
  segments(data);
  if(before!=acknowledged)
    Serial.printf("DISPLAY ACK=%d bits=%s\n",acknowledged,lastAck);
}
}


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




const char PAGE[] PROGMEM=R"HTML(
<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>自适应散热与状态监测系统</title>
<style>
:root{color-scheme:dark;--bg:#0a101b;--card:#121d2c;--line:#27364b;--text:#e8f0fc;--muted:#9bacc4;--green:#57dfbd;--blue:#74adff;--amber:#ffcb7a}
*{box-sizing:border-box}body{margin:0;background:radial-gradient(ellipse at top right,#183543 0,transparent 45%),var(--bg);color:var(--text);font:15px/1.6 system-ui,-apple-system,"Segoe UI",sans-serif}main{max-width:1120px;margin:auto;padding:30px 24px 50px}header{display:flex;justify-content:space-between;align-items:center;gap:20px;margin-bottom:24px}h1{font-size:25px;letter-spacing:.04em;margin:6px 0}h2{font-size:17px;margin:0 0 16px}.eyebrow{font-size:11px;letter-spacing:.19em;color:var(--green)}.muted,small{color:var(--muted)}.pill{border:1px solid var(--line);border-radius:100px;padding:6px 14px;white-space:nowrap;font-size:12px}.online{color:var(--green)}.offline{color:var(--amber)}.layout{display:grid;grid-template-columns:1fr 1fr;gap:18px}section{background:var(--card);border:1px solid var(--line);border-radius:20px;padding:24px;min-width:0}.wide{grid-column:1/-1}.hero{display:flex;align-items:center;justify-content:space-between;gap:15px}.temperature{font-size:54px;line-height:1.2;font-weight:650;font-variant-numeric:tabular-nums;margin:18px 0 12px;letter-spacing:-.04em}.fan{width:78px;height:78px;flex-shrink:0;border:1px solid #365b63;border-radius:50%;display:grid;place-items:center;background:#17353a;color:var(--green);font-size:35px}.fan span{display:block}.fan.running span{animation:spin 2s linear infinite}@keyframes spin{to{transform:rotate(360deg)}}.statline{display:flex;gap:16px;flex-wrap:wrap;font-size:13px}.notice{margin-top:18px;padding:12px 14px;background:#0c1725;border-left:3px solid var(--green);border-radius:7px;font-size:13px}.notice.warn{border-color:var(--amber);color:var(--amber)}.buttons{display:grid;grid-template-columns:repeat(3,1fr);gap:9px}button,.download{font:inherit;display:inline-block;border:1px solid #354860;background:#203048;color:var(--text);padding:11px 8px;border-radius:10px;text-decoration:none;cursor:pointer;text-align:center}button:hover,.download:hover{border-color:var(--green)}button.active{background:var(--green);color:#062d27;border-color:var(--green);font-weight:650}button:disabled{opacity:.35;cursor:not-allowed}button.stop{color:#ffb5b1;border-color:#714747;width:100%;margin-top:10px;background:#34242d}.label{font-size:12px;color:var(--muted);margin:16px 0 8px}.note{font-size:12px;color:var(--muted);margin:12px 0 0}.metrics{display:grid;grid-template-columns:repeat(4,1fr);gap:12px}.metric{padding:14px;background:#0b1624;border-radius:12px}.metric span{font-size:12px;color:var(--muted)}.metric b{display:block;font-size:25px;margin-top:5px;font-variant-numeric:tabular-nums}.charthead{display:flex;justify-content:space-between;gap:14px;flex-wrap:wrap}.legend{font-size:11px;display:flex;gap:12px;flex-wrap:wrap}.legend span:before{content:"";display:inline-block;width:7px;height:7px;border-radius:50%;margin-right:5px;background:var(--blue)}.legend span:nth-child(2):before{background:var(--green)}.legend span:nth-child(3):before{background:var(--amber)}canvas{display:block;width:100%;height:210px;margin-top:10px}progress{width:100%;height:8px;accent-color:var(--green);margin:12px 0 18px}.recordbar{display:flex;gap:10px;flex-wrap:wrap;align-items:center;justify-content:space-between}.download[aria-disabled=true]{opacity:.4;pointer-events:none}iframe{display:block;border:0;width:100%;height:45px;margin-top:12px;background:#101b2a;border-radius:8px}footer{font-size:12px;color:var(--muted);margin-top:20px}.good{color:var(--green)}:focus-visible{outline:2px solid var(--amber);outline-offset:3px}@media(max-width:760px){main{padding:20px 12px}.layout{grid-template-columns:1fr}.wide{grid-column:auto}header{align-items:flex-start}h1{font-size:21px}section{padding:18px}.metrics{grid-template-columns:repeat(2,1fr)}.temperature{font-size:44px}.pill{padding:5px 9px}button{font-size:13px}}@media(prefers-reduced-motion:reduce){.fan.running span{animation:none}}
</style></head><body><main>
<header><div><div class="eyebrow">THERMAL CONTROL · EXPERIMENT CONSOLE</div><h1>自适应散热与状态监测系统</h1><small>TL-C12C 大风扇 · 本地 Wi-Fi · 温度反馈调速</small></div><span id="connection" class="pill offline" role="status">正在连接</span></header>
<div class="layout">
<section><div class="hero"><div><div class="eyebrow">LIVE TEMPERATURE</div><div id="temp" class="temperature">-- °C</div></div><div id="fan" class="fan" aria-hidden="true"><span>✣</span></div></div>
<b id="rpm">RPM --</b><div id="state">等待设备状态</div><div class="statline muted"><span id="filtered">平滑温度 --</span><span id="rate">升温速率 --</span></div><div id="notice" class="notice" role="status">等待首次采样</div><p class="note">测速取自 IO33，暂按每转 2 脉冲估算。0 RPM 也可能是测速未接通。0% 是否停转看实物。</p></section>
<section><h2>运行控制</h2><form action="/control" method="post" target="result">
<div class="buttons"><button id="m0" name="value" value="manual">手动基线</button><button id="m1" name="value" value="threshold">阈值温控</button><button id="m2" name="value" value="trend">趋势补偿</button></div>
<div class="label">手动档位 · 选择后切换到手动模式</div><div class="buttons"><button id="s1" name="value" value="1">低档</button><button id="s2" name="value" value="2">中档</button><button id="s3" name="value" value="3">高档</button></div><button name="value" value="indicator_test">声光自检（短响）</button><button id="s0" class="stop" name="value" value="0">0% 输出 / 结束记录</button></form>
<p class="note">K1 短按循环手动档位，长按切换手动与上次自动策略。记录期间锁定网页、红外和按键的策略/档位修改；网页停止与遥控器 0 键始终可用。</p></section>
<section class="wide"><div class="charthead"><h2>温度响应</h2><div class="legend"><span>原始温度</span><span>平滑温度</span><span>补偿控制值</span></div></div><canvas id="temperature" aria-label="近期温度曲线"></canvas>
<div class="charthead"><h2>控制策略比较</h2><div class="legend"><span>目标档位</span><span>阈值建议</span><span>趋势建议</span></div></div><canvas id="levels" aria-label="目标档位与策略建议曲线"></canvas>
<p class="note">显示本页面最近 180 次状态；刷新页面会清空曲线。两种自动策略同步计算，只有选中策略驱动风扇。补偿控制值不是实测温度，也不是已验证的未来温度预测。</p></section>
<section class="wide"><h2>本次记录统计</h2><div class="metrics"><div class="metric"><span>最低原始温度</span><b id="minT">--</b></div><div class="metric"><span>最高原始温度</span><b id="maxT">--</b></div><div class="metric"><span>平均 PWM 指令</span><b id="meanOutput">--</b></div><div class="metric"><span>目标档位变化</span><b id="changes">--</b></div></div><p class="note">统计来自设备内存记录，刷新页面仍保留。平均 PWM 不是实测转速、功率或能耗。</p></section>
<section class="wide"><div class="recordbar"><h2>实验记录</h2><span id="record" class="muted">尚未读取</span></div><progress id="progress" max="600" value="0"></progress>
<form action="/control" method="post" target="result"><div class="buttons"><button name="value" value="start">开始新记录</button><button name="value" value="finish">结束记录</button><button name="value" value="clear">清空记录</button></div></form><p><a id="download" class="download" href="/experiment.csv" aria-disabled="true">导出 CSV</a></p>
<p class="note">开始新记录会清空上次数据；最多 600 个样本，约 10 分钟。到达容量或结束记录后风扇继续工作。数据仅存在 RAM，断电丢失，请先导出。CSV 导出过程中控制响应可能短暂延迟。</p>
<iframe name="result" id="result" title="设备操作反馈"></iframe></section></div>
<footer><span id="version">固件 --</span> · 页面 V11.1 · 无互联网依赖 · 手机断开后设备继续按当前模式运行</footer>
</main><script>
var points=[],lastMs=0;
function el(id){return document.getElementById(id);}
function draw(id,keys,fixed){
 var c=el(id),ctx=c.getContext('2d');c.width=900;c.height=210;
 var low=Infinity,high=-Infinity;points.forEach(function(p){keys.forEach(function(k){if(typeof p[k]==='number'&&isFinite(p[k])){low=Math.min(low,p[k]);high=Math.max(high,p[k]);}});});
 if(fixed){low=0;high=3;}else if(!isFinite(low)){low=20;high=30;}else{low=Math.floor(low-.5);high=Math.ceil(high+.5);}
 ctx.font='12px sans-serif';ctx.fillStyle='#9bacc4';
 for(var j=0;j<=3;j++){var y=15+j*50;ctx.fillText((high-(high-low)*j/3).toFixed(1),2,y+4);ctx.strokeStyle='#27364b';ctx.beginPath();ctx.moveTo(48,y);ctx.lineTo(890,y);ctx.stroke();}
 if(points.length<2)return;var first=points[0].ms,last=points[points.length-1].ms,colors=['#74adff','#57dfbd','#ffcb7a'];
 keys.forEach(function(k,i){ctx.beginPath();ctx.strokeStyle=colors[i];ctx.lineWidth=2;var connected=false;points.forEach(function(p){if(typeof p[k]!=='number'||!isFinite(p[k])){connected=false;return;}var x=48+(p.ms-first)/(last-first||1)*842,y=165-(p[k]-low)/(high-low||1)*150;if(connected)ctx.lineTo(x,y);else{ctx.moveTo(x,y);connected=true;}});ctx.stroke();});ctx.fillText('窗口 '+Math.round((last-first)/1000)+' 秒',48,200);ctx.fillText('现在',858,200);
}
function show(s){
 el("rpm").textContent=typeof s.rpm==="number"?s.rpm+" RPM":"RPM --";
 if(s.ms<lastMs)points=[];lastMs=s.ms;
 el('connection').textContent='设备在线';el('connection').className='pill online';
 el('temp').textContent=!s.sampled?'等待采样':s.raw===null?'采样无效':s.raw.toFixed(2)+' °C';
 el('state').textContent=['手动基线','阈值温控','趋势补偿'][s.mode]+' · '+['0% 指令','低档','中档','高档'][s.level]+' · PWM '+s.pwm+'/255';
 el('filtered').textContent='平滑 '+(s.filtered===null?'--':s.filtered.toFixed(2)+' °C');
 el('rate').textContent=s.rate===null?'速率窗口积累中（约 10 秒）':'速率 '+s.rate.toFixed(2)+' °C/min';
 el('fan').className=s.pwm>0?'fan running':'fan';
 var alarms=[];if(s.alarms&1)alarms.push('传感器故障');if(s.alarms&2)alarms.push('超温报警');
 el('notice').textContent=alarms.length?alarms.join(' / ')+(s.mode===0?'；当前手动模式，请检查设备':'；自动模式已请求全速'):!s.sampled?'等待首次采样':s.raw===null?'采样暂时无效；自动模式保持之前输出':'监测正常';el('notice').className=alarms.length||s.raw===null?'notice warn':'notice';
 ['minT','maxT'].forEach(function(k){el(k).textContent=s[k]===null?'--':s[k].toFixed(2)+' °C';});el('meanOutput').textContent=s.meanOutput===null?'--':s.meanOutput.toFixed(1)+'%';el('changes').textContent=s.count?s.changes+' 次':'--';
 var reasons={not_started:'尚未开始',recording:'记录中',manual_finish:'已结束',capacity_reached:'已达到容量',user_stop:'用户停止',cleared:'已清空'};
 el('record').textContent='#'+s.experimentId+' · '+(reasons[s.recordEndReason]||s.recordEndReason)+' · '+s.count+'/600';el('progress').value=s.count;el('version').textContent='固件 '+s.firmware+' · 数码管 '+(s.displayAck?'已应答':'未应答，请检查接线');
 document.querySelectorAll('button').forEach(function(b){b.classList.remove('active');b.disabled=s.recording&&['manual','threshold','trend','1','2','3','start','clear'].indexOf(b.value)>=0;if(b.value==='finish')b.disabled=!s.recording;});
 el('m'+s.mode).classList.add('active');el('s'+s.level).classList.add('active');el('download').setAttribute('aria-disabled',s.recording||!s.count?'true':'false');
 var p=Object.assign({},s);if(s.raw===null){p.baseline=null;p.trend=null;}points.push(p);if(points.length>180)points.shift();draw('temperature',['raw','filtered','predicted'],false);draw('levels',['level','baseline','trend'],true);
}
function fail(){el('connection').textContent='连接中断';el('connection').className='pill offline';el('notice').textContent='显示值已过时。请检查 Wi-Fi；断线期间网页停止指令无法送达，可使用遥控器 0 键或断开电源。';el('notice').className='notice warn';el('fan').className='fan';document.querySelectorAll('button').forEach(function(b){b.disabled=b.value!=='0';});el('download').setAttribute('aria-disabled','true');}
function poll(){var x=new XMLHttpRequest();x.open('GET','/status?t='+Date.now());x.timeout=4000;x.onload=function(){try{if(x.status!==200)throw Error();show(JSON.parse(x.responseText));}catch(e){fail();}};x.onerror=fail;x.ontimeout=fail;x.onloadend=function(){setTimeout(poll,1000);};x.send();}
document.querySelectorAll('button').forEach(function(b){b.addEventListener('click',function(e){if((b.value==='start'||b.value==='clear')&&!confirm('将清空上次记录，是否继续？'))e.preventDefault();});});
el('download').addEventListener('click',function(e){if(this.getAttribute('aria-disabled')==='true')e.preventDefault();});
el('result').addEventListener('load',function(){try{var d=this.contentDocument;if(d){d.body.style.cssText='margin:8px;color:#dce9f8;background:#101b2a;font:13px system-ui';}}catch(e){}});
poll();
</script></body></html>

)HTML";

const char* FIRMWARE_VERSION="11.2.0-tlc12c-display";
// Wiring: GPIO32 -> 1k -> NPN base, GPIO33 <- tach, GPIO27 -> K1, GPIO26 -> DS, GPIO14 -> IRED,
// GPIO21 -> D1, GPIO25 -> BEEP. Keep relay contacts disconnected.
const int MOTOR=32, BUTTON=27, DS=26, CHANNEL=0;
const int IR_PIN=14;
const int STATUS_LED_PIN=21, BEEP_PIN=25, BEEP_CH=2;
IRrecv irrecv(IR_PIN);
decode_results irResults;
const int DUTY[]={0,77,153,255};
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
int targetLevel=0, manualLevel=0, appliedPwm=0;
bool boosting=false;
uint32_t boostAt=0, conversionAt=0, rampAt=0;
int lastReading=HIGH, stableButton=HIGH;
uint32_t changedAt=0, pressedAt=0;
bool longHandled=false;

// RAM-only recording; stops at 600 samples. Export before power off.
constexpr int TACH_PIN=33, PULSES_PER_REV=2;
volatile uint32_t tachCount=0;
portMUX_TYPE tachLock=portMUX_INITIALIZER_UNLOCKED;
uint32_t tachAt=0, ledBeepAt=0;
bool testingIndicators=false;
int fanRpm=-1;
void IRAM_ATTR countPulse(){
  portENTER_CRITICAL_ISR(&tachLock); ++tachCount; portEXIT_CRITICAL_ISR(&tachLock);
}
void pollTach(){
  uint32_t now=millis(), elapsed=now-tachAt;
  if(elapsed<1000)return;
  portENTER_CRITICAL(&tachLock); uint32_t n=tachCount; tachCount=0; portEXIT_CRITICAL(&tachLock);
  tachAt=now;
  fanRpm=lroundf(n*60000.0f/(elapsed*float(PULSES_PER_REV)));
  Serial.printf("FAN PWM=%d%% pulses=%lu RPM=%d\n",(appliedPwm*100+127)/255,(unsigned long)n,fanRpm);
}
struct Record {
  uint32_t ms;
  float raw, filtered, rate, predicted;
  uint8_t mode, target, pwm, baseline, trend, valid, ready, alarms; int rpm;
};
const int CAPACITY=600;
Record records[CAPACITY];
int recordCount=0;
bool recording=false;
uint32_t recordAt=0;
uint32_t experimentId=0;
const char* recordEndReason="not_started";

void writePwm(int duty) {appliedPwm=duty;ledcWrite(CHANNEL,255-duty);}
void setSpeed(int level) {
  if(level<0 || level>3)return;
  if(level==0){targetLevel=0;boosting=false;writePwm(0);return;}
  if(level==targetLevel)return;
  bool stopped=targetLevel==0;
  targetLevel=level;
  if(!level){boosting=false;writePwm(0);}
  else if(stopped){boosting=true;boostAt=millis();writePwm(255);}
  // Running changes are handled by the nonblocking ramp.
  Serial.printf("Speed: %d\n",targetLevel);
}
void handleMotor(){
  uint32_t now=millis();
  if(boosting){
    if(now-boostAt<300)return;
    boosting=false;rampAt=now;return; // Smoothly descend from the startup boost.
  }
  if(now-rampAt<20)return;
  rampAt=now;
  int target=DUTY[targetLevel], delta=target-appliedPwm;
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
  if(recording)return; // All interfaces share the same experiment lock.
  if(selected==MANUAL && mode!=MANUAL)manualLevel=targetLevel;
  mode=selected;
  if(mode==MANUAL)setSpeed(manualLevel);
  else {lastAuto=mode;applyControl();}
}
void toggleMode(){selectMode(mode==MANUAL ? lastAuto : MANUAL);}
void shortPress(){
  if(recording)return;
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
void stopFan(){
  recording=false;recordEndReason="user_stop";mode=MANUAL;manualLevel=0;setSpeed(0);
}
void handleInfrared(){
  if(!irrecv.decode(&irResults))return;
  uint64_t code=irResults.value;
  bool repeat=irResults.repeat || code==0xFFFFFFFFFFFFFFFFULL;
  if(!repeat && irResults.decode_type==NEC && irResults.bits==32){
    if(recording && code!=0xFF6897ULL){irrecv.resume();return;}
    Serial.printf("IR NEC 0x%llX\n",(unsigned long long)code);
    switch(code){
      case 0xFF30CFULL: selectMode(THRESHOLD);break; // 1: threshold control
      case 0xFF18E7ULL: selectMode(TREND);break;     // 2: trend control
      case 0xFF7A85ULL: selectMode(MANUAL);break;    // 3: manual mode
      case 0xFF6897ULL:                              // 0: stop
        stopFan();break;
      case 0xFF02FDULL:                              // *: one manual step down
        selectMode(MANUAL);if(manualLevel>0)--manualLevel;setSpeed(manualLevel);break;
      case 0xFFC23DULL:                              // #: one manual step up
        selectMode(MANUAL);if(manualLevel<3)++manualLevel;setSpeed(manualLevel);break;
      default: break; // Other keys deliberately have no action.
    }
  }
  irrecv.resume();
}
void handleIndicators(){
  const uint32_t now=millis();
  if(testingIndicators && now-ledBeepAt>=450)testingIndicators=false;
  if(testingIndicators){digitalWrite(STATUS_LED_PIN,HIGH);ledcWrite(BEEP_CH,128);return;}
  const bool alarm=alarmBits()!=0;
  // Solid D1 = running; blinking D1 = confirmed sensor/high-temperature alarm.
  digitalWrite(STATUS_LED_PIN,alarm ? ((now/350)%2==0) : (targetLevel>0));
  // The passive buzzer runs briefly every 4s. LEDC channel 2 has a different
  // timer from channel 0 so its 2 kHz tone does not alter fan PWM (25 kHz).
  ledcWrite(BEEP_CH,alarm && now%4000<180 ? 128 : 0);
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
      ctl.predicted,(uint8_t)mode,(uint8_t)targetLevel,(uint8_t)appliedPwm,
      (uint8_t)ctl.thresholdLevel,(uint8_t)ctl.trendLevel,
      (uint8_t)ctl.valid,(uint8_t)ctl.ready,alarmBits(),fanRpm};
    if(recordCount==CAPACITY){recording=false;recordEndReason="capacity_reached";}
  }
  Serial.printf("[%s] T=%.2f filtered=%.2f rate=%.2f valid=%d level=%d PWM=%d\n",
    MODE_NAMES[mode],rawTemperature,ctl.filtered,ctl.rate,ctl.valid,targetLevel,appliedPwm);
  sensors.requestTemperatures();conversionAt=millis();
}
String numberOrNull(float v){return ctl.valid ? String(v,2) : "null";}
void sendStatus(){
  String s;s.reserve(720);
  s="{\"ms\":"+String(millis())+",\"mode\":"+String((int)mode);
  s+=",\"raw\":"+numberOrNull(rawTemperature);
  s+=",\"filtered\":"+numberOrNull(ctl.filtered);
  s+=",\"predicted\":"+numberOrNull(ctl.predicted);
  s+=",\"rate\":"+(ctl.ready ? String(ctl.rate,2) : String("null"));
  s+=",\"level\":"+String(targetLevel)+",\"pwm\":"+String(appliedPwm);
  s+=",\"baseline\":"+String(ctl.thresholdLevel);
  s+=",\"trend\":"+String(ctl.trendLevel);
  s+=",\"recording\":"+String(recording ? "true" : "false");
  s+=",\"alarms\":"+String(alarmBits());
  s+=",\"sampled\":"+String(sampled?"true":"false");
  s+=",\"firmware\":\""+String(FIRMWARE_VERSION)+"\",\"schemaVersion\":1";
  s+=",\"experimentId\":"+String(experimentId);
  s+=",\"recordEndReason\":\""+String(recordEndReason)+"\"";
  s+=",\"rpm\":"+(fanRpm<0?String("null"):String(fanRpm));
  s+=",\"displayAck\":"+String(TempDisplay::acknowledged?"true":"false"); 
  int validCount=0, changes=0;float lo=INFINITY,hi=-INFINITY;uint32_t pwmSum=0;
  for(int i=0;i<recordCount;i++){
    const Record& r=records[i];pwmSum+=r.pwm;
    if(r.valid){validCount++;if(r.raw<lo)lo=r.raw;if(r.raw>hi)hi=r.raw;}
    if(i && r.target!=records[i-1].target)changes++;
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
  if(recording){server.send(409,"text/plain; charset=utf-8","请先结束记录再导出");return;}
  if(!recordCount){server.send(404,"text/plain; charset=utf-8","还没有可导出的记录");return;}
  server.sendHeader("Cache-Control","no-store");
  server.sendHeader("Content-Disposition","attachment; filename=fan_experiment.csv");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200,"text/csv; charset=utf-8","");
  server.sendContent("elapsed_ms,raw_C,filtered_C,rate_C_per_min,predicted_C,mode,target_level,pwm_command,threshold_suggestion,trend_suggestion,sensor_valid,trend_ready,alarm_bits,firmware,experiment_id,rpm_est\r\n");
  String chunk;chunk.reserve(1800);
  for(int i=0;i<recordCount;i++){
    const Record& r=records[i];
    chunk+=String(r.ms)+",";
    if(r.valid)chunk+=String(r.raw,3);chunk+=",";
    if(r.valid)chunk+=String(r.filtered,3);chunk+=",";
    if(r.ready)chunk+=String(r.rate,3);chunk+=",";
    if(r.valid)chunk+=String(r.predicted,3);
    chunk+=","+String(MODE_NAMES[r.mode])+","+String(r.target)+","+String(r.pwm);
    chunk+=","+String(r.baseline)+","+String(r.trend)+","+String(r.valid);
    chunk+=","+String(r.ready)+","+String(r.alarms)+","+String(FIRMWARE_VERSION)+","+String(experimentId)+","+(r.rpm<0?String(""):String(r.rpm))+"\r\n";
    if(chunk.length()>1400){server.sendContent(chunk);chunk="";yield();}
  }
  if(chunk.length())server.sendContent(chunk);
  server.sendContent("");
}

void handleControl(){
  String v=server.arg("value"),message="操作成功";
  server.sendHeader("Cache-Control","no-store");
  Serial.println("Web command: "+v);
  if(recording && (v=="manual" || v=="threshold" || v=="trend" || v=="1" || v=="2" || v=="3" || v=="clear")){
    server.send(409,"text/plain; charset=utf-8","请先结束记录，再切换策略、档位或清空数据");return;
  }
  if(v=="indicator_test"){testingIndicators=true;ledBeepAt=millis();message="LED + BEEP test";}
  else if(v=="0"){stopFan();message="已输出0%并结束记录，请观察实际转速";}
  else if(v=="manual")selectMode(MANUAL);
  else if(v=="threshold")selectMode(THRESHOLD);
  else if(v=="trend")selectMode(TREND);
  else if(v=="1" || v=="2" || v=="3"){
    mode=MANUAL;manualLevel=v.toInt();setSpeed(manualLevel);
  }else if(v=="start"){
    if(recording)message="正在记录，未重新开始";
    else{recordCount=0;recordAt=millis();++experimentId;recording=true;recordEndReason="recording";message="新实验已开始，上次记录已清空";}
  }else if(v=="finish"){
    if(recording){recording=false;recordEndReason="manual_finish";}
    message="记录已结束，风扇继续按当前模式工作";
  }else if(v=="clear"){recordCount=0;recordEndReason="cleared";message="记录已清空";}
  else{server.send(400,"text/plain; charset=utf-8","无效指令");return;}
  server.send(200,"text/plain; charset=utf-8",message);
}
void setup(){
  Serial.begin(115200);Serial.println(FIRMWARE_VERSION);pinMode(BUTTON,INPUT_PULLUP);
  digitalWrite(STATUS_LED_PIN,LOW);pinMode(STATUS_LED_PIN,OUTPUT);
  digitalWrite(BEEP_PIN,LOW);pinMode(BEEP_PIN,OUTPUT);
  ledcSetup(BEEP_CH,2000,8);ledcAttachPin(BEEP_PIN,BEEP_CH);ledcWrite(BEEP_CH,0);
  ledcSetup(CHANNEL,25000,8);ledcAttachPin(MOTOR,CHANNEL);writePwm(0);
  sensors.begin();sensors.setResolution(12);sensors.setWaitForConversion(false);
  sensors.requestTemperatures();conversionAt=millis();
  pinMode(TACH_PIN,INPUT_PULLUP); tachAt=millis();
  attachInterrupt(digitalPinToInterrupt(TACH_PIN),countPulse,FALLING);
  TempDisplay::begin();
  irrecv.enableIRIn();
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
  Serial.print("TL-C12C integrated test: http://");Serial.println(WiFi.softAPIP());
}
void loop(){
  handleButton();
  handleInfrared();
  handleMotor();pollTach();
  handleTemperature();
  TempDisplay::update(rawTemperature,sampled && ctl.valid);
  handleIndicators();server.handleClient();
}

