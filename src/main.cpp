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
#include "dashboard_page.h"
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






const char* FIRMWARE_VERSION="12.0.0-smartfan-iot";
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


void sendRecords(){
  int offset=server.hasArg("offset") ? server.arg("offset").toInt() : 0;
  int limit=server.hasArg("limit") ? server.arg("limit").toInt() : 25;
  if(offset<0)offset=0;
  if(limit<1)limit=25;
  if(limit>25)limit=25;
  String filter=server.arg("filter");
  int total=0;
  for(int i=0;i<recordCount;i++){
    const Record& r=records[i];
    bool match=true;
    if(filter=="alarm")match=r.alarms!=0;
    else if(filter=="invalid")match=!r.valid;
    else if(filter=="hot")match=r.valid && r.raw>=ALARM_HIGH_C;
    if(match)++total;
  }
  String s;s.reserve(4200);
  s="{\"total\":"+String(total)+",\"offset\":"+String(offset)+",\"limit\":"+String(limit)+",\"items\":[";
  int matched=0,emitted=0;
  for(int i=0;i<recordCount;i++){
    const Record& r=records[i];
    bool match=true;
    if(filter=="alarm")match=r.alarms!=0;
    else if(filter=="invalid")match=!r.valid;
    else if(filter=="hot")match=r.valid && r.raw>=ALARM_HIGH_C;
    if(!match)continue;
    if(matched++<offset)continue;
    if(emitted>=limit)break;
    if(emitted++)s+=",";
    s+="{\"index\":"+String(i+1)+",\"elapsedMs\":"+String(r.ms);
    s+=",\"raw\":";if(r.valid)s+=String(r.raw,2);else s+="null";
    s+=",\"filtered\":";if(r.valid)s+=String(r.filtered,2);else s+="null";
    s+=",\"rate\":";if(r.ready)s+=String(r.rate,2);else s+="null";
    s+=",\"predicted\":";if(r.valid)s+=String(r.predicted,2);else s+="null";
    s+=",\"mode\":\""+String(MODE_NAMES[r.mode])+"\"";
    s+=",\"target\":"+String(r.target)+",\"pwm\":"+String(r.pwm);
    s+=",\"baseline\":"+String(r.baseline)+",\"trend\":"+String(r.trend);
    s+=",\"valid\":"+String(r.valid?"true":"false");
    s+=",\"ready\":"+String(r.ready?"true":"false");
    s+=",\"alarms\":"+String(r.alarms);
    s+=",\"rpm\":";if(r.rpm>=0)s+=String(r.rpm);else s+="null";
    s+="}";
  }
  s+="]}";
  server.sendHeader("Cache-Control","no-store");
  server.send(200,"application/json",s);
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
  server.on("/records",HTTP_GET,sendRecords);
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

