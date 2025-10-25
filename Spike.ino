#include <EasyLogger.h>
#include "config.h"
#include "BluetoothSerial.h"
#include <WiFi.h>

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

BluetoothSerial SerialBT;

enum State {
  Config, // The device is allowing bluetooth connection to connect to a wifi network
  Ready,  // The device is ready to intercept traffic
  Alive,  // The device is intercepting traffic
  Killed, // The device was stopped
  None
};

bool BTisConnected = false;

void callback(esp_spp_cb_event_t event, esp_spp_cb_param_t*param){
  if(event == ESP_SPP_SRV_OPEN_EVT){
    LOG_INFO("BT_Callback", "BT connected!");
    SerialBT.println("READY");
    BTisConnected = true;
  } else {
    BTisConnected = false;
  }
}

State state = None;
State pastState = None;

char* state_name(State s) {
  char* states[] = { "Config", "Ready", "Alive", "Killed", "None" };
  return states[s];
}

char* bt_incoming_buffer = (char*)malloc(INPUT_BUFFER_SIZE * sizeof(char));
int current_index = 0;

void clear_bt_input_buffer() {
  memset(bt_incoming_buffer, 0, INPUT_BUFFER_SIZE);
  current_index = 0;
}

bool buffer_comp(char* a, char* b, int length) {
  for(int i = 0; i < length; ++i) {
    if (a[i] != b[i]) return false;  
  }
  return true;
}

enum dataType {
  Command,
  SSID,
  Password
};

enum dataType expecting = Command;

char* network_ssid = (char*)malloc(INPUT_BUFFER_SIZE * sizeof(char));
char* network_pass = (char*)malloc(INPUT_BUFFER_SIZE * sizeof(char));

void command_callback() {
  LOG_DEBUG("Command_callback", expecting);
  if (expecting == Command && buffer_comp("set_ssid", bt_incoming_buffer, 8)) {
    LOG_INFO("command_callback", "Awaiting ssid");
    expecting = SSID;
  } else if (expecting == SSID) {
    memcpy(network_ssid, bt_incoming_buffer, sizeof(char) * INPUT_BUFFER_SIZE);
    expecting = Command;
  } else if (expecting == Command && buffer_comp("get_ssid", bt_incoming_buffer, 8)) {
    SerialBT.print("Current SSID: ");
    SerialBT.println(network_ssid);
  } if (expecting == Command && buffer_comp("set_pass", bt_incoming_buffer, 8)) {
    LOG_INFO("command_callback", "Awaiting password");
    expecting = Password;
  } else if (expecting == Password) {
    memcpy(network_pass, bt_incoming_buffer, sizeof(char) * INPUT_BUFFER_SIZE);
    expecting = Command;
  } else if (expecting == Command && buffer_comp("connect", bt_incoming_buffer, 8)) {
    SerialBT.println("Goodbye!");
    SerialBT.disconnect();
    SerialBT.end();
    state = Ready;
  } else {
    SerialBT.println("Invalid command!");
    LOG_WARNING("command_callback", "Invalid Command!");
  }
}

void handle_bt() {
 if (SerialBT.available()) {
  if (current_index >= INPUT_BUFFER_SIZE) {
    LOG_EMERGENCY("handle_bt", "Buffer overflow!");
    state = Killed;
    return;
  }

  char input = SerialBT.read();

  if (input == ';') {
    LOG_INFO("handle_bt", "Got input: " << bt_incoming_buffer);
    command_callback();
    clear_bt_input_buffer();
  } else {
    bt_incoming_buffer[current_index] = input;
    ++current_index;
  }
 }
}

void setup_network() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  WiFi.setHostname(DEVICE_NAME);
  WiFi.begin(network_ssid, network_pass);
  LOG_INFO("setup_network", "Connecting to network");
  while (WiFi.status() != WL_CONNECTED) {
    LOG_INFO("setup_network", "Not yet connected...");
    delay(1000);
  }
  LOG_INFO("setup_network", "Connected! Local IP: " << WiFi.localIP());
  state = Alive;
}

void setup() {
  Serial.begin(115200);
  SerialBT.begin(DEVICE_NAME);
  SerialBT.register_callback(callback);
  clear_bt_input_buffer();
  
  state = Config;
  LOG_INFO("Setup", "Initialised!");
}

void loop() {
  if (pastState != state) {
    LOG_INFO("Loop", "State changed! " << state_name(pastState) << " -> " << state_name(state));
    pastState = state;
  }

  switch (state) {
    case Config:
      handle_bt();
    break;
    case Ready:
      setup_network();
    break;
    case Alive:
    break;
    case Killed:
      LOG_INFO("Loop", "Killed, entering loop state.");
      for (;;);
    break;
    default:
      LOG_EMERGENCY("Loop", "Invalid state!");
      state = Killed;
      break;
  }
}
