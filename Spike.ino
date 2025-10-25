#include <EasyLogger.h>
#include "config.h"
#include "BluetoothSerial.h"

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
    clear_bt_input_buffer();
    return;
  }

  bt_incoming_buffer[current_index] = input;
  ++current_index;
 }
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
