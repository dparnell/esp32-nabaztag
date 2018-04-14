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

static int wifi_initialized = 0;
static int wifi_status = 1;  // RT2501_S_IDLE

#if CONFIG_FREERTOS_UNICORE
#define NET_RUNNING_CORE 0
#else
#define NET_RUNNING_CORE 1
#endif


static xQueueHandle _network_event_queue;
static TaskHandle_t _network_event_task_handle = NULL;

const char * system_event_reasons[] = { "UNSPECIFIED", "AUTH_EXPIRE", "AUTH_LEAVE", "ASSOC_EXPIRE", "ASSOC_TOOMANY", "NOT_AUTHED", "NOT_ASSOCED", "ASSOC_LEAVE", "ASSOC_NOT_AUTHED", "DISASSOC_PWRCAP_BAD", "DISASSOC_SUPCHAN_BAD", "IE_INVALID", "MIC_FAILURE", "4WAY_HANDSHAKE_TIMEOUT", "GROUP_KEY_UPDATE_TIMEOUT", "IE_IN_4WAY_DIFFERS", "GROUP_CIPHER_INVALID", "PAIRWISE_CIPHER_INVALID", "AKMP_INVALID", "UNSUPP_RSN_IE_VERSION", "INVALID_RSN_IE_CAP", "802_1X_AUTH_FAILED", "CIPHER_SUITE_REJECTED", "BEACON_TIMEOUT", "NO_AP_FOUND", "AUTH_FAIL", "ASSOC_FAIL", "HANDSHAKE_TIMEOUT" };
#define reason2str(r) ((r>176)?system_event_reasons[r-177]:system_event_reasons[r-1])

static void _network_event_task(void * arg){
    system_event_t *event = NULL;
    for (;;) {
        if(xQueueReceive(_network_event_queue, &event, portMAX_DELAY) == pdTRUE) {
          if(event->event_id == SYSTEM_EVENT_SCAN_DONE) {
            printf("*** SYSTEM_EVENT_SCAN_DONE\n");
            wifi_status = 1; // RT2501_S_IDLE
          } else if(event->event_id == SYSTEM_EVENT_STA_DISCONNECTED) {
            uint8_t reason = event->event_info.disconnected.reason;
            printf("*** SYSTEM_ECENT_STA_DISCONNECTED: Reason: %u - %s\n", reason, reason2str(reason));
            wifi_status = 1; // RT2501_S_IDLE
          } else if(event->event_id == SYSTEM_EVENT_AP_START) {
            printf("*** SYSTEM_EVENT_AP_START\n");
            wifi_status = 5; // RT2501_S_MASTER
          } else if(event->event_id == SYSTEM_EVENT_AP_STOP) {
            printf("*** SYSTEM_EVENT_AP_STOP\n");
            wifi_status = 1; // RT2501_S_IDLE
          } else if(event->event_id == SYSTEM_EVENT_STA_START) {
            printf("*** SYSTEM_EVENT_STA_START\n");
            wifi_status = 1; // RT2501_S_IDLE
          } else if(event->event_id == SYSTEM_EVENT_STA_STOP) {
            printf("*** SYSTEM_EVENT_STA_STOP\n");
            wifi_status = 1; // RT2501_S_IDLE
          } else if(event->event_id == SYSTEM_EVENT_STA_CONNECTED) {
            printf("*** SYSTEM_EVENT_STA_CONNECTED\n");
            wifi_status = 3; // RT2501_S_CONNECTING
          } else if(event->event_id == SYSTEM_EVENT_STA_GOT_IP) {
            printf("*** SYSTEM_EVENT_STA_GOT_IP\n");
            wifi_status = 4; // RT2501_S_CONNECTED
          } else {
            printf("*** UNKNOWN SYSTEM EVENT: %d\n", event->event_id);
          }
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
  tcpip_adapter_init();
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_err_t err = esp_wifi_init(&cfg);
  if(err){
    printf("esp_wifi_init %d", err);
  } else {
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

static wifi_config_t wifi_config;

void netSetmode(int mode, char* ssid, int _chn)
{
  wifi_initialized = 1;

  // stop any existing WiFi connection
  // ESP_ERROR_CHECK(esp_wifi_stop());

  wifi_init_config_t wifiInitializationConfig = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&wifiInitializationConfig);
  esp_wifi_set_storage(WIFI_STORAGE_RAM);

  if(mode == 1) {
    printf("netSetMode: %d - %s\n", mode, ssid);
    // set access point mode

    tcpip_adapter_ip_info_t info;
    IP4_ADDR(&info.ip, 192, 168, 0, 1);
    IP4_ADDR(&info.gw, 192, 168, 0, 1);
    IP4_ADDR(&info.netmask, 255, 255, 255, 0);
    tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP);
    ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_AP, &info));
    ESP_ERROR_CHECK(tcpip_adapter_dhcps_start(TCPIP_ADAPTER_IF_AP));

    wifi_config_t ap_config;

    strcpy((char*)&ap_config.ap.ssid[0], ssid);
    ap_config.ap.ssid_len = strlen(ssid);
    ap_config.ap.channel = 0;
    ap_config.ap.authmode = WIFI_AUTH_OPEN;
    ap_config.ap.ssid_hidden = 0;
    ap_config.ap.max_connection = 4;
    ap_config.ap.beacon_interval = 100;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());
  } else {
    printf("netSetMode: %d\n", mode);
    // set station mode
    tcpip_adapter_dhcps_stop(TCPIP_ADAPTER_IF_AP);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());

    int countdown = 6000;
    wifi_status = 3;
    printf("Connecting.");
    while(countdown-- && wifi_status == 3) {
      vTaskDelay(100 / portTICK_PERIOD_MS);
      printf(".");
      esp_task_wdt_reset();
    }

    printf("\n");
  }

}

static uint16_t nscan = 10;
static wifi_ap_record_t records[10];

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

    char *ssid = (char*)&records[i].ssid[0];
    VPUSH(PNTTOVAL(VMALLOCSTR(ssid,strlen(ssid))));
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
  memset(&wifi_config, 0, sizeof(wifi_config));
  strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
  strncpy((char*)wifi_config.sta.password, key, sizeof(wifi_config.sta.password));
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
  strncpy(buf, key, 32);
}

int netRssi()
{
    wifi_ap_record_t info;
    if(!esp_wifi_sta_get_ap_info(&info)) {
        return info.rssi;
    }

    return 0;
}
