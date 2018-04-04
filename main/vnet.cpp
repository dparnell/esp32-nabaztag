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
        if(xQueueReceive(_network_event_queue, &event, portMAX_DELAY) == pdTRUE){
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
  wifi_mode_t mode;
  esp_wifi_get_mode(&mode);
  if(mode == WIFI_MODE_AP) {
    return 5; // RT2501_S_MASTER
  }

  return wifi_status;
  /*
  int status = WiFi.status();
  // printf("netState: %d\n", status);

  switch(status) {
    case WL_NO_SHIELD:
      return 1; // RT2501_S_IDLE
    case WL_IDLE_STATUS:
      return 1; // RT2501_S_IDLE
    case WL_NO_SSID_AVAIL:
      return 0; // RT2501_S_BROKEN
    case WL_SCAN_COMPLETED:
      return 2; // RT2501_S_SCAN
    case WL_CONNECTED:
      return 4; // RT2501_S_CONNECTED
    case WL_CONNECT_FAILED:
      return 0; // RT2501_S_BROKEN
    case WL_CONNECTION_LOST:
      return 3; // RT2501_S_CONNECTING
    case WL_DISCONNECTED:
      return 0; // RT2501_S_BROKEN
    default:
      return 0; // RT2501_S_BROKEN
  }
  */
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
    // WiFi.mode(WIFI_STA);
  }

    ESP_ERROR_CHECK(esp_wifi_start());  
}

void netScan(char* ssid)
{
  int nscan = 0 ; // WiFi.scanNetworks();
  for(int i = 0; i < nscan; i++) {
    /*
    const char* ssid = WiFi.SSID(i).c_str();
    uint8_t* bssid = WiFi.BSSID(i);

    VPUSH(PNTTOVAL(VMALLOCSTR((char*)ssid, strlen(ssid))));
    VPUSH(PNTTOVAL(VMALLOCSTR((char*)bssid,6)));
    VPUSH(PNTTOVAL(VMALLOCSTR((char*)bssid,6)));
    VPUSH(INTTOVAL(WiFi.RSSI(i)));
    VPUSH(INTTOVAL(WiFi.channel(i)));
    VPUSH(INTTOVAL(1)); // rateset
    VPUSH(INTTOVAL(WiFi.encryptionType(i)));
    VMKTAB(7);
    */
  }
  VPUSH(NIL);
  while(nscan--) VMKTAB(2);
}

void netAuth(char* ssid, char* mac, char* bssid, int chn, int rate, int authmode, int encrypt, char* key)
{
  // WiFi.begin(ssid, key);
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
