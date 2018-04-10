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
#include "esp_wifi_internal.h"
#include <esp_event_loop.h>
#include "esp_task_wdt.h"


extern "C" system_event_handler_t default_event_handlers[SYSTEM_EVENT_MAX];

static int wifi_initialized = 0;
static int wifi_status = 1;  // RT2501_S_IDLE
static wifi_interface_t wifi_interface = ESP_IF_WIFI_STA;


const char * system_event_reasons[] = { "UNSPECIFIED", "AUTH_EXPIRE", "AUTH_LEAVE", "ASSOC_EXPIRE", "ASSOC_TOOMANY", "NOT_AUTHED", "NOT_ASSOCED", "ASSOC_LEAVE", "ASSOC_NOT_AUTHED", "DISASSOC_PWRCAP_BAD", "DISASSOC_SUPCHAN_BAD", "IE_INVALID", "MIC_FAILURE", "4WAY_HANDSHAKE_TIMEOUT", "GROUP_KEY_UPDATE_TIMEOUT", "IE_IN_4WAY_DIFFERS", "GROUP_CIPHER_INVALID", "PAIRWISE_CIPHER_INVALID", "AKMP_INVALID", "UNSUPP_RSN_IE_VERSION", "INVALID_RSN_IE_CAP", "802_1X_AUTH_FAILED", "CIPHER_SUITE_REJECTED", "BEACON_TIMEOUT", "NO_AP_FOUND", "AUTH_FAIL", "ASSOC_FAIL", "HANDSHAKE_TIMEOUT" };
#define reason2str(r) ((r>176)?system_event_reasons[r-177]:system_event_reasons[r-1])

static esp_err_t _network_event_cb(void *arg, system_event_t *event){
  if(event->event_id == SYSTEM_EVENT_SCAN_DONE) {
    printf("***SYSTEM_EVENT_SCAN_DONE\n");
    wifi_status = 1; // RT2501_S_IDLE
  } else if(event->event_id == SYSTEM_EVENT_STA_DISCONNECTED) {
    uint8_t reason = event->event_info.disconnected.reason;
    printf("***SYSTEM_EVENT_STA_DISCONNECTED: Reason: %u - %s\n", reason, reason2str(reason));
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
    printf("***SYSTEM_EVENT_AP_START\n");
    wifi_status = 5; // RT2501_S_MASTER
  } else if(event->event_id == SYSTEM_EVENT_AP_STOP) {
    printf("***SYSTEM_EVENT_AP_STOP\n");
    wifi_status = 1; // RT2501_S_IDLE
  } else if(event->event_id == SYSTEM_EVENT_STA_START) {
    printf("***SYSTEM_EVENT_STA_START\n");
    esp_wifi_connect();
    wifi_status = 3; // RT2501_S_IDLE
  } else if(event->event_id == SYSTEM_EVENT_STA_STOP) {
    printf("***SYSTEM_EVENT_STA_STOP\n");
    wifi_status = 1; // RT2501_S_IDLE
  } else if(event->event_id == SYSTEM_EVENT_STA_CONNECTED) {
    printf("***SYSTEM_EVENT_STA_CONNECTED\n");
    wifi_status = 4; // RT2501_S_CONNECTING
  } else if(event->event_id == SYSTEM_EVENT_STA_GOT_IP) {
    printf("***SYSTEM_EVENT_STA_GOT_IP\n");
    wifi_status = 4; // RT2501_S_CONNECTED
  }
  return ESP_OK;
}

esp_err_t wifi_rx_cb(void *buffer, uint16_t length, void *eb) {
  char dummy[6];
  memset(dummy, 0, sizeof(dummy));
  netCb((char*)buffer, length, dummy);

  if(eb != NULL) {
    esp_wifi_internal_free_rx_buffer(eb);
  }

  return ESP_OK;
}

static esp_err_t ignore_event(system_event_t *event) {
  printf("Ignoring event: %d\n", event->event_id);
  // _network_event_cb(NULL, event);

  return ESP_OK;
}

static esp_err_t wifi_station_connect(system_event_t *event) {
  return esp_wifi_internal_reg_rxcb(ESP_IF_WIFI_STA, wifi_rx_cb);
}

static esp_err_t wifi_station_disconnect(system_event_t *event) {
  return esp_wifi_internal_reg_rxcb(ESP_IF_WIFI_STA, NULL);
}

static esp_err_t wifi_ap_connect(system_event_t *event) {
  return esp_wifi_internal_reg_rxcb(ESP_IF_WIFI_AP, wifi_rx_cb);
}

static esp_err_t wifi_ap_disconnect(system_event_t *event) {
  return esp_wifi_internal_reg_rxcb(ESP_IF_WIFI_AP, NULL);
}

void netInit() {
  printf("Initializing WiFi network settings...\n");
  wifi_initialized = 0;
  wifi_status = 1; // IDLE

  esp_event_loop_init(&_network_event_cb, NULL);

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  //Register event handlers
  default_event_handlers[SYSTEM_EVENT_STA_START] = ignore_event;
  default_event_handlers[SYSTEM_EVENT_STA_STOP] = ignore_event;
  default_event_handlers[SYSTEM_EVENT_STA_CONNECTED] = wifi_station_connect;
  default_event_handlers[SYSTEM_EVENT_STA_DISCONNECTED] = wifi_station_disconnect;
  default_event_handlers[SYSTEM_EVENT_STA_GOT_IP] = ignore_event;
  default_event_handlers[SYSTEM_EVENT_STA_LOST_IP] = ignore_event;
  default_event_handlers[SYSTEM_EVENT_AP_START] = wifi_ap_connect;
  default_event_handlers[SYSTEM_EVENT_AP_STOP] = wifi_ap_disconnect;

  //Register shutdown handler
  ESP_ERROR_CHECK(esp_register_shutdown_handler((shutdown_handler_t) esp_wifi_stop));

  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));

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
  esp_wifi_internal_tx(wifi_interface, &src[indexsrc], lentosend);
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
  ESP_ERROR_CHECK(esp_wifi_stop());

  wifi_init_config_t wifiInitializationConfig = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&wifiInitializationConfig));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

  if(mode == 1) {
    printf("netSetMode: %d - %s\n", mode, ssid);
    // set access point mode
    wifi_interface = ESP_IF_WIFI_AP;

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
    wifi_interface = ESP_IF_WIFI_STA;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());

    wifi_status = 3;
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
  printf("Scan finished scan - found %d records\n", nscan);

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
    printf("\t%s - %d %d\n", records[i].ssid, records[i].rssi, enc_type);
  }
  VPUSH(NIL);
  while(nscan--) VMKTAB(2);
  printf("Scan done\n");
}

void netAuth(char* ssid, char* mac, char* bssid, int chn, int rate, int authmode, int encrypt, char* key)
{
  printf("netAuth: %s - %d %d\n", ssid, authmode, encrypt);
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
