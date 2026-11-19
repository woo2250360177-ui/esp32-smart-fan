#include "cloud_bridge.h"
#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif
#define BLYNK_PRINT Serial
#define BLYNK_FIRMWARE_VERSION "13.0.0"
#define BLYNK_RUN_YIELD() delay(1)
#include <BlynkSimpleEsp32_SSL.h>
#include <atomic>
#include <time.h>
#include <freertos/queue.h>
#include <freertos/task.h>

namespace CloudBridge {
namespace {
QueueHandle_t commandQueue=nullptr, stopQueue=nullptr, stateQueue=nullptr;
std::atomic<bool> online{false};
std::atomic<uint32_t> generation{0};
bool refreshState=false;
constexpr uint32_t demoWindowMs=10UL*60*1000;
constexpr uint32_t demoIntervalMs=5000;
constexpr uint32_t regularIntervalMs=180000;
constexpr uint32_t cloudConnectBudgetMs=30000;

void sendState(const State& now, const State& sent, bool all, bool measurementsDue){
  if(measurementsDue || (now.sensorValid && !sent.sensorValid)){
    if(now.sensorValid)Blynk.virtualWrite(V0,now.temperature);
  }
  if(now.rpm>=0 && (measurementsDue || sent.rpm<0))Blynk.virtualWrite(V1,now.rpm);
  if(all || now.pwmPercent!=sent.pwmPercent)Blynk.virtualWrite(V2,now.pwmPercent);
  if(all || now.mode!=sent.mode)Blynk.virtualWrite(V3,now.mode);
  if(all || now.level!=sent.level)Blynk.virtualWrite(V4,now.level);
  if(all || now.alarms!=sent.alarms)Blynk.virtualWrite(V5,now.alarms);
}

// The cloud task owns Blynk and passes copies to the Arduino loop.
void networkTask(void*){
  Blynk.config(FAN_BLYNK_AUTH_TOKEN,FAN_BLYNK_SERVER,443);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(FAN_WIFI_SSID,FAN_WIFI_PASSWORD);
  Serial.println("[Blynk] Joining configured Wi-Fi");

  State latest{}, sent{};
  bool haveState=false, wifiWasUp=false, clockReady=false, demoStarted=false;
  uint32_t wifiAt=millis(), retryAt=millis()-10000, logAt=millis();
  uint32_t demoStartedAt=0, sendAt=0, measurementAt=0;

  for(;;){
    if(xQueueReceive(stateQueue,&latest,0)==pdTRUE)haveState=true;
    const uint32_t now=millis();
    if(WiFi.status()!=WL_CONNECTED){
      online.store(false);
      if(wifiWasUp){
        Blynk.disconnect();
        Serial.println("[Blynk] Wi-Fi lost");
        wifiWasUp=false;
      }
      if(now-wifiAt>=15000){
        wifiAt=now;
        Serial.printf("[Blynk] Waiting for Wi-Fi, status=%d\n",int(WiFi.status()));
        WiFi.reconnect();
      }
    }else{
      if(!wifiWasUp){
        wifiWasUp=true;
        retryAt=now-10000;
        Serial.printf("[Blynk] Wi-Fi connected, IP=%s\n",WiFi.localIP().toString().c_str());
        if(!clockReady)configTime(0,0,"ntp.aliyun.com","time.cloudflare.com","pool.ntp.org");
      }
      if(!clockReady && time(nullptr)>1700000000){
        clockReady=true;
        Serial.println("[Blynk] Time synchronized");
      }
      if(!clockReady && now-logAt>=15000){
        logAt=now;
        Serial.println("[Blynk] Waiting for NTP before TLS");
      }
      if(clockReady && !Blynk.connected() && now-retryAt>=10000){
        // DNS and TLS can wait here while the local control loop continues.
        IPAddress address;
        Serial.printf("[Blynk] DNS lookup: %s, RSSI=%d dBm\n",
            FAN_BLYNK_SERVER,static_cast<int>(WiFi.RSSI()));
        const uint32_t dnsStartedAt=millis();
        const bool dnsResolved=WiFi.hostByName(FAN_BLYNK_SERVER,address)==1;
        const uint32_t dnsMs=millis()-dnsStartedAt;
        Serial.printf("[Blynk] DNS %s, elapsed=%lu ms\n",
            dnsResolved?address.toString().c_str():"failed",static_cast<unsigned long>(dnsMs));

        // The Blynk timeout includes the TLS connection and the login reply.
        Serial.printf("[Blynk] Cloud connect budget=%lu ms\n",
            static_cast<unsigned long>(cloudConnectBudgetMs));
        const uint32_t connectStartedAt=millis();
        const bool connected=Blynk.connect(cloudConnectBudgetMs);
        retryAt=millis();
        Serial.printf("[Blynk] Cloud attempt %s, elapsed=%lu ms, Wi-Fi status=%d\n",
            connected?"succeeded":"failed",static_cast<unsigned long>(retryAt-connectStartedAt),
            static_cast<int>(WiFi.status()));
        if(!connected && dnsResolved){
          char errorText[160]{};
          const int error=_blynkWifiClient.lastError(errorText,sizeof(errorText));
          if(error<0)Serial.printf("[Blynk] Last secure socket error=%d: %s\n",error,errorText);
        }
      }
      if(Blynk.connected())Blynk.run();
      if(!Blynk.connected() && online.exchange(false)){
        Serial.println("[Blynk] Cloud disconnected");
      }

      const uint32_t sendNow=millis();
      if(online.load() && haveState && (refreshState || sendNow-sendAt>=1000)){
        if(!demoStarted){
          demoStarted=true;
          demoStartedAt=sendNow;
        }
        const uint32_t interval=sendNow-demoStartedAt<demoWindowMs?demoIntervalMs:regularIntervalMs;
        const bool measurementsDue=refreshState || sendNow-measurementAt>=interval;
        sendState(latest,sent,refreshState,measurementsDue);
        sent=latest;
        if(measurementsDue)measurementAt=sendNow;
        refreshState=false;
        sendAt=sendNow;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
}  // namespace

void onConnect(){
  generation.fetch_add(1);
  refreshState=true;
  online.store(true);
  // Report the current device state without asking for stored commands.
  Serial.println("[Blynk] Cloud ONLINE");
}

void receive(const char* value, char max, int offset){
  if(!online.load() || !value || value[0]<'0' || value[0]>max || value[1]!='\0')return;
  const Command command{offset+value[0]-'0',millis(),generation.load()};
  if(command.code==0)xQueueOverwrite(stopQueue,&command);
  else if(xQueueSend(commandQueue,&command,0)!=pdTRUE)Serial.println("[Blynk] Command queue full");
}

bool current(const Command& command){
  return online.load() && command.generation==generation.load() && millis()-command.receivedAt<=2000;
}

bool take(Command& command){
  if(!commandQueue)return false;
  if(xQueueReceive(stopQueue,&command,0)==pdTRUE){
    // A new stop supersedes pending changes to speed or mode.
    if(current(command)){
      Command discarded;
      while(xQueueReceive(commandQueue,&discarded,0)==pdTRUE){}
    }
    return true;
  }
  return xQueueReceive(commandQueue,&command,0)==pdTRUE;
}

void publish(const State& state){
  if(stateQueue)xQueueOverwrite(stateQueue,&state);
}

void begin(){
  if(!FAN_BLYNK_AUTH_TOKEN[0] || !FAN_WIFI_SSID[0]){
    Serial.println("[Blynk] Fill include/secrets.h to enable cloud");
    return;
  }
  commandQueue=xQueueCreate(8,sizeof(Command));
  stopQueue=xQueueCreate(1,sizeof(Command));
  stateQueue=xQueueCreate(1,sizeof(State));
  if(commandQueue && stopQueue && stateQueue &&
     xTaskCreatePinnedToCore(networkTask,"fan_cloud",12288,nullptr,1,nullptr,0)==pdPASS)return;
  if(commandQueue)vQueueDelete(commandQueue);
  if(stopQueue)vQueueDelete(stopQueue);
  if(stateQueue)vQueueDelete(stateQueue);
  commandQueue=stopQueue=stateQueue=nullptr;
  Serial.println("[Blynk] Cloud task unavailable");
}
}  // namespace CloudBridge

BLYNK_CONNECTED(){CloudBridge::onConnect();}
BLYNK_WRITE(V6){CloudBridge::receive(param.asStr(),'3',0);}
BLYNK_WRITE(V7){CloudBridge::receive(param.asStr(),'2',4);}
