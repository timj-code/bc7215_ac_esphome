#pragma once

#include <memory>
#include <string>

#include "driver/gpio.h"
#include "driver/uart.h"

#include "esphome/core/component.h"
#include "esphome/core/preferences.h"
#include "esphome/components/climate/climate.h"

// Provided by the bc7215_ac_lib repository's ESP-IDF example component:
// deps/bc7215_ac_lib/examples/ESP-IDF/components/bc7215ac
#include "bc7215ac.hpp"

namespace esphome {
namespace bc7215_ac {

class BC7215ACClimate : public climate::Climate, public Component {
 public:
  void set_uart_num(int uart_num) { this->uart_num_ = static_cast<uart_port_t>(uart_num); }
  void set_tx_pin(int pin) { this->tx_pin_ = static_cast<gpio_num_t>(pin); }
  void set_rx_pin(int pin) { this->rx_pin_ = static_cast<gpio_num_t>(pin); }
  void set_busy_pin(int pin) { this->busy_pin_ = static_cast<gpio_num_t>(pin); }
  void set_mod_pin(int pin) { this->mod_pin_ = static_cast<gpio_num_t>(pin); }
  void set_library_unit_celsius(bool celsius) { this->library_unit_celsius_ = celsius; }

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  climate::ClimateTraits traits() override;

  // Called by HA/API/climate automations.
  void control(const climate::ClimateCall &call) override;

  // These methods can be called from YAML lambdas:
  //   id(bedroom_ac).start_pairing();
  //   id(bedroom_ac).builtin_or_next();
  void start_pairing();
  void stop_pairing();
  void builtin_or_next();

  // Helper methods for text_sensor/display lambdas.
  std::string status_message() const { return this->status_message_; }
  bool is_paired() const;
  bool is_pairing() const { return this->pairing_; }
  uint8_t predefined_count() const;

 protected:
  enum class PairState : uint8_t {
    IDLE,
    PAIRING,
  };

  enum class ParsingState : uint8_t {
    IDLE,
    PARSING,
	STOP,
  };

  struct StoredPairingData {
    bc7215DataMaxPkt_t data;
    bc7215FormatPkt_t format;
    uint8_t match_index;
	ParsingState parsing_state;
    bool library_unit_celsius;
  };

  // Increment this when StoredPairingData changes. This avoids loading
  // binary data saved by an older firmware with a different struct layout.
  static constexpr uint32_t kPreferenceVersion = 2;

  void set_status_(const std::string &message);
  void handle_pairing_loop_();
  void handle_parsing_loop_();
  void mark_unpaired_();

  bool load_pairing_();
  bool save_pairing_();

  bool try_next_predefined_();
  bool try_match_next_();
  bool send_current_state_();
  bool send_test_command_();

  int map_mode_to_bc7215_(climate::ClimateMode mode) const;
  int map_fan_to_bc7215_() const;
  climate::ClimateMode map_bc7215_mode_to_ha_(int m) const;
  climate::ClimateFanMode map_bc7215_fan_to_ha_(int f) const;

  int climate_temp_to_library_temp_(float temp_c) const;
  float library_temp_to_climate_temp_(int t) const; 
  void update_action_from_mode_();

  uart_port_t uart_num_{UART_NUM_1};
  gpio_num_t tx_pin_{GPIO_NUM_NC};
  gpio_num_t rx_pin_{GPIO_NUM_NC};
  gpio_num_t busy_pin_{GPIO_NUM_NC};
  gpio_num_t mod_pin_{GPIO_NUM_NC};

  bool library_unit_celsius_{true};

  ESPPreferenceObject pref_;

  std::unique_ptr<bc7215::BC7215AC> ac_;

  PairState pair_state_{PairState::IDLE};
  ParsingState parsing_state_{ParsingState::STOP};
  bool pairing_{false};
  uint8_t predefined_number_{0};
  uint8_t match_index_{0};
  uint8_t ac_pressed_key_{0};

  std::string status_message_{"Not initialized."};
};

}  // namespace bc7215_ac
}  // namespace esphome
