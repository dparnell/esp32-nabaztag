// VLISP Virtual Machine - 2006 - by Sylvain Huet
// Lowcost IS Powerfull
#include "vmem.h"
#include "properties.h"
#include <string.h>

#include "vloader.h"
#include "vnet.h"
#include "vlog.h"
#include "vinterp.h"

#include "esp_wifi.h"
#include <esp_event_loop.h>
#include "esp_task_wdt.h"


extern "C" system_event_handler_t default_event_handlers[SYSTEM_EVENT_MAX];

static int wifi_initialized = 0;
static int wifi_status = 1;  // RT2501_S_IDLE

#if CONFIG_FREERTOS_UNICORE
#define NET_RUNNING_CORE 0
#else
#define NET_RUNNING_CORE 1
#endif


static xQueueHandle _network_event_queue;
static TaskHandle_t _network_event_task_handle = NULL;

static void _network_event_task(void * arg){
  system_event_t *event = NULL;
  for (;;) {
    if(xQueueReceive(_network_event_queue, &event, portMAX_DELAY) == pdTRUE) {
      if(event->event_id == SYSTEM_EVENT_SCAN_DONE) {
        wifi_status = 1; // RT2501_S_IDLE
      } else if(event->event_id == SYSTEM_EVENT_STA_DISCONNECTED) {
        uint8_t reason = event->event_info.disconnected.reason;
        // log_w("Reason: %u - %s", reason, reason2str(reason));
        if(reason == WIFI_REASON_NO_AP_FOUND) {
          wifi_status = 0; // RT2501_S_BROKEN
        } else if(reason == WIFI_REASON_AUTH_FAIL || reason == WIFI_REASON_ASSOC_FAIL) {
          wifi_status = 0; // RT2501_S_BROKEN
        } else if(reason == WIFI_REASON_BEACON_TIMEOUT || reason == WIFI_REASON_HANDSHAKE_TIMEOUT) {
          wifi_status = 3; // RT2501_S_CONNECTING
        } else if(reason == WIFI_REASON_AUTH_EXPIRE) {
          wifi_status = 3; // RT2501_S_CONNECTING
        } else {
          wifi_status = 0; // RT2501_S_BROKEN
        }

        esp_wifi_connect();
      } else if(event->event_id == SYSTEM_EVENT_AP_START) {
        wifi_status = 5; // RT2501_S_MASTER
      } else if(event->event_id == SYSTEM_EVENT_AP_STOP) {
        wifi_status = 1; // RT2501_S_IDLE
      } else if(event->event_id == SYSTEM_EVENT_STA_START) {
        esp_wifi_connect();
        wifi_status = 1; // RT2501_S_IDLE
      } else if(event->event_id == SYSTEM_EVENT_STA_STOP) {
        wifi_status = 1; // RT2501_S_IDLE
      } else if(event->event_id == SYSTEM_EVENT_STA_CONNECTED) {
        wifi_status = 3; // RT2501_S_CONNECTING
      } else if(event->event_id == SYSTEM_EVENT_STA_GOT_IP) {
        wifi_status = 4; // RT2501_S_CONNECTED
      }
      // WiFiGenericClass::_eventCallback(arg, event);
    }
  }
  vTaskDelete(NULL);
  _network_event_task_handle = NULL;
}

static esp_err_t _network_event_cb(void *arg, system_event_t *event){
  if (xQueueSend(_network_event_queue, &event, portMAX_DELAY) != pdPASS) {
    printf("Network Event Queue Send Failed!\n");
    return ESP_FAIL;
  }
  return ESP_OK;
}

static void _start_network_event_task(){
  if(!_network_event_queue){
    _network_event_queue = xQueueCreate(32, sizeof(system_event_t *));
    if(!_network_event_queue){
      printf("Network Event Queue Create Failed!\n");
      return;
    }
  }
  if(!_network_event_task_handle){
    xTaskCreatePinnedToCore(_network_event_task, "network_event", 4096, NULL, 2, &_network_event_task_handle, NET_RUNNING_CORE);
    if(!_network_event_task_handle){
      printf("Network Event Task Start Failed!\n");
      return;
    }
  }
  esp_event_loop_init(&_network_event_cb, NULL);
}

void netInit() {
  wifi_initialized = 0;
  wifi_status = 1; // IDLE

  _start_network_event_task();
  // tcpip_adapter_init();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_err_t err = esp_wifi_init(&cfg);
  if(err){
    printf("esp_wifi_init %d", err);
  } else {

    //Register event handlers
    default_event_handlers[SYSTEM_EVENT_STA_START] = NULL; //esp32WifiStaStartEvent;
    default_event_handlers[SYSTEM_EVENT_STA_STOP] = NULL; //esp32WifiStaStopEvent;
    default_event_handlers[SYSTEM_EVENT_STA_CONNECTED] = NULL; // esp32WifiStaConnectedEvent;
    default_event_handlers[SYSTEM_EVENT_STA_DISCONNECTED] = NULL; // esp32WifiStaDisconnectedEvent;
    default_event_handlers[SYSTEM_EVENT_STA_GOT_IP] = NULL; // esp32WifiStaGotIpEvent;
    default_event_handlers[SYSTEM_EVENT_STA_LOST_IP] = NULL; // esp32WifiStaLostIpEvent;
    default_event_handlers[SYSTEM_EVENT_AP_START] = NULL; // esp32WifiApStartEvent;
    default_event_handlers[SYSTEM_EVENT_AP_STOP] = NULL; // esp32WifiApStopEvent;

    //Register shutdown handler
    esp_register_shutdown_handler((shutdown_handler_t) esp_wifi_stop);


    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_wifi_set_mode(WIFI_MODE_NULL);
  }
}

int netState()
{
  if(wifi_initialized) {
    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);
    if(mode == WIFI_MODE_AP) {
      return 5; // RT2501_S_MASTER
    }
  }

  return wifi_status;
}

int netSend(char* src,int indexsrc,int lentosend,int lensrc,char* macdst,int inddst,int lendst,int speed)
{
  // NOT IMPLEMENTED
  return 0;
}

int netCb(char* src,int lensrc,char* macsrc)
{
  VPUSH(PNTTOVAL(VMALLOCSTR(src,lensrc)));
  VPUSH(PNTTOVAL(VMALLOCSTR(macsrc,6)));
  VPUSH(VCALLSTACKGET(sys_start,SYS_CBTCP));
  if (VSTACKGET(0)!=NIL) interpGo();
  else { VDROP();VDROP();}
  VDROP();
  return 0;
}

uint8_t mac[6];

char* netMac()
{
  // make sure that the WiFi stuff is initialized first
  esp_wifi_get_mac(WIFI_IF_STA, mac);

  return (char*)mac;
}

int netChk(char* src, int indexsrc, int lentosend, int lensrc, unsigned int val)
{
  unsigned short* p;

  if (indexsrc < 0) return val;
  if (indexsrc + lentosend > lensrc) lentosend = lensrc - indexsrc;
  if (lentosend <= 0) return val;

  src += indexsrc;
  p=(unsigned short*)src;

  val=((val << 8) & 0xff00) + ((val >> 8) & 0xff);
  while(lentosend > 1)
    {
      val += *(p++);
      lentosend -= 2;
    }

  if (lentosend) val += *(unsigned char*)p;

  val = (val >> 16) + (val & 0xffff);
  val = (val >> 16) + (val & 0xffff);
  val = ((val << 8) & 0xff00) + ((val >> 8) & 0xff);
  return val;
}

void netSetmode(int mode, char* ssid, int _chn)
{
  wifi_initialized = 1;
  printf("netSetMode: %d - %s\n", mode, ssid);

  wifi_init_config_t wifiInitializationConfig = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&wifiInitializationConfig);
  esp_wifi_set_storage(WIFI_STORAGE_RAM);

  if(mode == 1) {
    // set access point mode

    tcpip_adapter_ip_info_t info;
    IP4_ADDR(&info.ip, 192, 168, 0, 1);
    IP4_ADDR(&info.gw, 192, 168, 0, 1);
    IP4_ADDR(&info.netmask, 255, 255, 255, 0);
    ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP));
    ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &info));
    ESP_ERROR_CHECK(tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP));

    wifi_config_t ap_config;

    strcpy((char*)&ap_config.ap.ssid[0], ssid);
    ap_config.ap.channel = 0;
    ap_config.ap.authmode = WIFI_AUTH_OPEN;
    ap_config.ap.ssid_hidden = 0;
    ap_config.ap.max_connection = 4;
    ap_config.ap.beacon_interval = 100;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
  } else {
    // set station mode
    ESP_ERROR_CHECK(tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  }

  ESP_ERROR_CHECK(esp_wifi_start());
}

void netScan(char* ssid)
{
  wifi_init_config_t wifiInitializationConfig = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&wifiInitializationConfig);
  esp_wifi_set_storage(WIFI_STORAGE_RAM);

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_start());

  wifi_scan_config_t config;
  config.ssid = 0;
  config.bssid = 0;
  config.channel = 0;
  config.show_hidden = false;
  config.scan_type = WIFI_SCAN_TYPE_ACTIVE;
  config.scan_time.active.min = 100;
  config.scan_time.active.max = 300;
  wifi_status = 2; // RT2501_S_SCAN
  ESP_ERROR_CHECK(esp_wifi_scan_start(&config, false));

  printf("Initiating wifi scan - %d", wifi_status);
  while(wifi_status == 2) {
    vTaskDelay(100 / portTICK_PERIOD_MS);
    printf(".");
    esp_task_wdt_reset();
  }
  printf("\n");

  uint16_t nscan = 10;
  wifi_ap_record_t records[10];

  esp_wifi_scan_get_ap_num(&nscan);
  if(nscan > 10) {
    nscan = 10;
  }

  esp_wifi_scan_get_ap_records(&nscan, records);

  for(int i = 0; i < nscan; i++) {
    int enc_type;
    switch(records[i].authmode) {
    case WIFI_AUTH_OPEN:
      enc_type = 0; // none
      break;
    case WIFI_AUTH_WEP:
      enc_type = 2; // WEP128
      break;
    case WIFI_AUTH_WPA_PSK:
    case WIFI_AUTH_WPA2_PSK:
    case WIFI_AUTH_WPA_WPA2_PSK:
    case WIFI_AUTH_WPA2_ENTERPRISE:
      enc_type = 3; // WPA
      break;
    default:
      enc_type = 4; // unsupported
    }

    VPUSH(PNTTOVAL(VMALLOCSTR((char*)records[i].bssid,6)));
    VPUSH(PNTTOVAL(VMALLOCSTR((char*)records[i].bssid,6)));
    VPUSH(INTTOVAL(records[i].rssi));
    VPUSH(INTTOVAL(records[i].primary));
    VPUSH(INTTOVAL(1)); // rateset
    VPUSH(INTTOVAL(enc_type));
    VMKTAB(7);
  }
  VPUSH(NIL);
  while(nscan--) VMKTAB(2);
}

void netAuth(char* ssid, char* mac, char* bssid, int chn, int rate, int authmode, int encrypt, char* key)
{
  wifi_config_t wifi_config;

  strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
  strncpy((char*)wifi_config.sta.password, key, sizeof(wifi_config.sta.password));

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
  ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
  ESP_ERROR_CHECK(esp_wifi_start() );
}

void netSeqAdd(unsigned char* seq,int n)
{
  unsigned char res[4];
  unsigned int val;
  val=(seq[0]<<24)+(seq[1]<<16)+(seq[2]<<8)+seq[3];
  val+=n;
  res[3]=val; val>>=8;
  res[2]=val; val>>=8;
  res[1]=val; val>>=8;
  res[0]=val;
  VPUSH(PNTTOVAL(VMALLOCSTR((char*)res,4)));
}

void netPmk(char* ssid, char* key, char* buf)
{
  // NOT IMPLEMENTED
  //printf("xxxx netPmk %s %s\n",ssid,key);
  //strcpy(buf,"01234567012345670123456701234567");
}

int netRssi()
{
  wifi_ap_record_t info;
  if(!esp_wifi_sta_get_ap_info(&info)) {
    return info.rssi;
  }

  return 0;
}
