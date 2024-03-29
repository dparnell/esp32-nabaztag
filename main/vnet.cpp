// VLISP Virtual Machine - 2006 - by Sylvain Huet
// Lowcost IS Powerfull
#include "vmem.h"
#include "properties.h"
#include <string.h>

#include "vloader.h"
#include "vnet.h"
#include "vlog.h"
#include "vinterp.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include <esp_event.h>
#include "esp_task_wdt.h"
#include "esp_wifi_netif.h"
#include "esp_netif_net_stack.h"

//#define DUMP_PACKETS

#define TAG "VNET"

extern int lockInterp();
extern void unlockInterp();

static int wifi_initialized = 0;
static int wifi_status = 1;  // RT2501_S_IDLE
static wifi_interface_t wifi_interface = WIFI_IF_STA;
static char wifi_ssid[32];
static char wifi_password[64];

static wifi_netif_driver_t sta_driver = NULL;
static esp_netif_t *netif_sta = NULL;
static wifi_netif_driver_t ap_driver = NULL;
static esp_netif_t *netif_ap = NULL;

static const char BROADCAST_MACID[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
static uint8_t mac[6];

const char * system_event_reasons[] = { "UNSPECIFIED", "AUTH_EXPIRE", "AUTH_LEAVE", "ASSOC_EXPIRE", "ASSOC_TOOMANY", "NOT_AUTHED", "NOT_ASSOCED", "ASSOC_LEAVE", "ASSOC_NOT_AUTHED", "DISASSOC_PWRCAP_BAD", "DISASSOC_SUPCHAN_BAD", "IE_INVALID", "MIC_FAILURE", "4WAY_HANDSHAKE_TIMEOUT", "GROUP_KEY_UPDATE_TIMEOUT", "IE_IN_4WAY_DIFFERS", "GROUP_CIPHER_INVALID", "PAIRWISE_CIPHER_INVALID", "AKMP_INVALID", "UNSUPP_RSN_IE_VERSION", "INVALID_RSN_IE_CAP", "802_1X_AUTH_FAILED", "CIPHER_SUITE_REJECTED", "BEACON_TIMEOUT", "NO_AP_FOUND", "AUTH_FAIL", "ASSOC_FAIL", "HANDSHAKE_TIMEOUT" };
#define reason2str(r) ((r>176)?system_event_reasons[r-177]:system_event_reasons[r-1])


#ifdef DUMP_PACKETS
void hex_dump(const char *title, const void *data, size_t size) {
	char ascii[17];
	size_t i, j;
	ascii[16] = '\0';

  printf("%s | ", title);
	for (i = 0; i < size; ++i) {
		printf("%02X ", ((unsigned char*)data)[i]);
		if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~') {
			ascii[i % 16] = ((unsigned char*)data)[i];
		} else {
			ascii[i % 16] = '.';
		}
		if ((i+1) % 8 == 0 || i+1 == size) {
			printf(" ");
			if ((i+1) % 16 == 0) {
				printf("|  %s \n", ascii);
        if(i < size) {
          printf("%s | ", title);
        }
			} else if (i+1 == size) {
				ascii[(i+1) % 16] = '\0';
				if ((i+1) % 16 <= 8) {
					printf(" ");
				}
				for (j = (i+1) % 16; j < 16; ++j) {
					printf("   ");
				}
				printf("|  %s \n", ascii);
        if(i < size-1) {
          printf("%s | ", title);
        }
			}
		}
	}
  printf("-----------------------------\n");
}
#endif

esp_err_t wifi_rx_cb(esp_netif_t *esp_netif, void *buf, size_t length, void *eb) {
  char *buffer = (char*)buf;
#ifdef DUMP_PACKETS
  hex_dump("wifi_rx_cb", buffer, length);
#endif

  netCb((char*)&buffer[6], length-6, (char*)&buffer[6]);
  if(eb) {
    esp_netif_free_rx_buffer(esp_netif, eb);
  }
  //play_check(0);
  //rec_check();

  return ESP_OK;
}

static esp_err_t _network_event_cb(void *arg, system_event_t *event){
  if(event->event_id == SYSTEM_EVENT_SCAN_DONE) {
    printf("***SYSTEM_EVENT_SCAN_DONE\n");
    wifi_status = 1; // RT2501_S_IDLE
  } else if(event->event_id == SYSTEM_EVENT_STA_DISCONNECTED) {
    uint8_t reason = event->event_info.disconnected.reason;
    printf("***SYSTEM_EVENT_STA_DISCONNECTED: Reason: %u - %s\n", reason, reason2str(reason));
    wifi_status = 1; // RT2501_S_IDLE
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
  } else if(event->event_id == SYSTEM_EVENT_AP_STACONNECTED) {
    printf("****SYSTEM_EVENT_AP_STACONNECTED\n");
  } else if(event->event_id == SYSTEM_EVENT_AP_STADISCONNECTED) {
    printf("****SYSTEM_EVENT_AP_STADISCONNECTED\n");
  } else {
    printf("*** UNKNOWN SYSTEM EVENT: %d\n", event->event_id);
  }
  return ESP_OK;
}

void netInit() {
  printf("Initializing WiFi network settings...\n");
  wifi_initialized = 0;
  wifi_status = 1; // IDLE

  ESP_ERROR_CHECK(esp_netif_init());
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK( esp_wifi_init(&cfg) );

  ESP_ERROR_CHECK(esp_event_loop_init(&_network_event_cb, NULL));

  //Register shutdown handler
  ESP_ERROR_CHECK(esp_register_shutdown_handler((shutdown_handler_t) esp_wifi_stop));

  printf("Registering Wifi station driver...\n");
  esp_netif_config_t cfg_sta = ESP_NETIF_DEFAULT_WIFI_STA();
  netif_sta = esp_netif_new(&cfg_sta);
  assert(netif_sta);
  ESP_ERROR_CHECK(esp_netif_attach_wifi_station(netif_sta));
  // ESP_ERROR_CHECK(esp_wifi_set_default_wifi_sta_handlers());
  sta_driver = (wifi_netif_driver_t)esp_netif_get_io_driver(netif_sta);
  ESP_ERROR_CHECK(esp_wifi_register_if_rxcb(sta_driver, wifi_rx_cb, netif_sta));

  printf("Registering Wifi AP driver...\n");
  esp_netif_config_t cfg_ap = ESP_NETIF_DEFAULT_WIFI_AP();
  netif_ap = esp_netif_new(&cfg_ap);
  assert(netif_ap);
  ESP_ERROR_CHECK(esp_netif_attach_wifi_ap(netif_ap));
  // ESP_ERROR_CHECK(esp_wifi_set_default_wifi_ap_handlers());
  ap_driver = (wifi_netif_driver_t)esp_netif_get_io_driver(netif_ap);
  ESP_ERROR_CHECK(esp_wifi_register_if_rxcb(ap_driver, wifi_rx_cb, netif_ap));

  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));

  ESP_ERROR_CHECK(esp_wifi_start());
  esp_wifi_connect();
}

int netState()
{
  if(wifi_initialized) {
    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);
    if(mode == WIFI_MODE_AP) {
      wifi_status = 5; // RT2501_S_MASTER
    }
  }

  // printf("netState: %d - %d\n", wifi_initialized, wifi_status);

  return wifi_status;
}

static unsigned char wifi_buffer[2048];

int netSend(char* src,int indexsrc,int lentosend,int lensrc,char* macdst,int inddst,int lendst,int speed)
{
  if (indexsrc<0) return -1;
  if (indexsrc+lentosend>lensrc) lentosend=lensrc-indexsrc;
  if (lentosend<=0) return -1;
  if (inddst<0) return -1;
  if (inddst+6>lendst) return -1;

  // printf("netSend: %p, %d, %d, %d, %p, %d, %d, %d\n", src, indexsrc, lentosend, lensrc, macdst, inddst, lendst, speed);

  // put in the destination mac
  memcpy(wifi_buffer, macdst + inddst, lendst);
  // copy the buffer from the vm
  memcpy(wifi_buffer + lendst, src + indexsrc, lentosend);
  // overwrite source macid in the buffer with the real one
  memcpy(wifi_buffer + 6, netMac(), 6);

  #ifdef DUMP_PACKETS
  hex_dump("netSend", wifi_buffer, lentosend + 12);
  #endif

  if(wifi_status == 4) {
    esp_netif_transmit(netif_sta, wifi_buffer, lentosend + 12);
  } else if(wifi_status == 5) {
    esp_netif_transmit(netif_ap, wifi_buffer, lentosend + 12);
  }

  return 0;
}

int netCb(char* src,int lensrc,char* macsrc)
{
  lockInterp();
  VPUSH(PNTTOVAL(VMALLOCSTR(src,lensrc)));
  VPUSH(PNTTOVAL(VMALLOCSTR(macsrc,6)));
  VPUSH(VCALLSTACKGET(sys_start,SYS_CBTCP));
  if (VSTACKGET(0)!=NIL) interpGo();
  else { VDROP();VDROP();}
  VDROP();
  unlockInterp();

  return 0;
}

char* netMac()
{
  wifi_mode_t mode;
  esp_wifi_get_mode(&mode);
  if(mode == WIFI_MODE_AP) {
    esp_wifi_get_mac(WIFI_IF_AP, mac);
  } else {
    esp_wifi_get_mac(WIFI_IF_STA, mac);
  }

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
  // stop any existing WiFi connection
  // ESP_ERROR_CHECK(esp_wifi_stop());

  if(!wifi_initialized) {
    wifi_init_config_t wifiInitializationConfig = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifiInitializationConfig));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
  }

  if(mode == 1) {
    printf("netSetMode: %d - %s\n", mode, ssid);

    // set access point mode
    if(wifi_interface != WIFI_IF_AP || !wifi_initialized) {
      wifi_interface = WIFI_IF_AP;

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
    }
  } else {
    printf("netSetMode: %d - %s : %s\n", mode, wifi_ssid, wifi_password);

    // set station mode
    if(wifi_interface != WIFI_IF_STA || !wifi_initialized) {
      wifi_config_t wifi_config;
      memset(&wifi_config, 0, sizeof(wifi_config));

      strncpy((char*)wifi_config.sta.ssid, wifi_ssid, sizeof(wifi_config.sta.ssid));
      strncpy((char*)wifi_config.sta.password, wifi_password, sizeof(wifi_config.sta.password));

      wifi_interface = WIFI_IF_STA;

      ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
      ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
      ESP_ERROR_CHECK(esp_wifi_start());
      ESP_ERROR_CHECK(esp_wifi_connect());

  /*
      int countdown = 600;
      wifi_status = 3;
      printf("Connecting.");
      while(countdown-- && wifi_status == 3) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        printf(".");
        esp_task_wdt_reset();
      }
  */
    }
  }
  wifi_initialized = 1;
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

  if(ssid) {
    strncpy(wifi_ssid, ssid, sizeof(wifi_ssid));
  } else {
    memset(wifi_ssid, 0, sizeof(wifi_ssid));
  }
  if(key) {
    strncpy(wifi_password, key, sizeof(wifi_password));
  } else {
    memset(wifi_password, 0, sizeof(wifi_password));
  }

  // use the given auth
  netSetmode(0, NULL, 0);
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
