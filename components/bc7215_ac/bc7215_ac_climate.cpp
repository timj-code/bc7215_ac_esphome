#include "bc7215_ac_climate.h"

#include <cmath>
#include <cstring>

#include "esphome/core/log.h"

namespace esphome {
namespace bc7215_ac {

static const char *const TAG = "bc7215_ac.climate";

void BC7215ACClimate::setup() {
  ESP_LOGCONFIG(TAG, "Setting up BC7215 AC climate adapter...");

  this->ac_ = std::make_unique<bc7215::BC7215AC>(
      this->uart_num_,
      this->bc7215_rx_pin_,
      this->bc7215_tx_pin_,
      this->busy_pin_,
      this->mod_pin_);

  const esp_err_t err = this->ac_->begin();
  if (err != ESP_OK) {
    this->set_status_(std::string("BC7215AC begin failed: ") + esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  if (this->library_unit_celsius_) {
    this->ac_->set_celsius();
  } else {
    this->ac_->set_fahrenheit();
  }

  this->mode = climate::CLIMATE_MODE_OFF;
  this->target_temperature = 25.0f;
  this->action = climate::CLIMATE_ACTION_OFF;
  this->set_fan_mode_(climate::CLIMATE_FAN_AUTO);
  this->power_on_ = false;

  this->pref_ = this->make_entity_preference<StoredPairingData>(kPreferenceVersion);
  this->load_pairing_();

  this->publish_state();
}

void BC7215ACClimate::loop() {
  this->handle_pairing_loop_();
  this->handle_parsing_loop_();
}

void BC7215ACClimate::dump_config() {
  ESP_LOGCONFIG(TAG, "BC7215 AC Climate:");
  ESP_LOGCONFIG(TAG, "  UART: %d", static_cast<int>(this->uart_num_));
  ESP_LOGCONFIG(TAG, "  BC7215 TX pin: GPIO%d", static_cast<int>(this->bc7215_tx_pin_));
  ESP_LOGCONFIG(TAG, "  BC7215 RX pin: GPIO%d", static_cast<int>(this->bc7215_rx_pin_));
  ESP_LOGCONFIG(TAG, "  BUSY pin: GPIO%d", static_cast<int>(this->busy_pin_));
  ESP_LOGCONFIG(TAG, "  MOD pin: GPIO%d", static_cast<int>(this->mod_pin_));
  ESP_LOGCONFIG(TAG, "  Library temperature unit: %s", this->library_unit_celsius_ ? "Celsius" : "Fahrenheit");
  ESP_LOGCONFIG(TAG, "  Paired: %s", this->is_paired() ? "yes" : "no");
}

climate::ClimateTraits BC7215ACClimate::traits() {
  auto traits = climate::ClimateTraits();

  traits.add_supported_mode(climate::CLIMATE_MODE_OFF);
  traits.add_supported_mode(climate::CLIMATE_MODE_AUTO);
  traits.add_supported_mode(climate::CLIMATE_MODE_COOL);
  traits.add_supported_mode(climate::CLIMATE_MODE_HEAT);
  traits.add_supported_mode(climate::CLIMATE_MODE_DRY);
  traits.add_supported_mode(climate::CLIMATE_MODE_FAN_ONLY);

  traits.add_supported_fan_mode(climate::CLIMATE_FAN_AUTO);
  traits.add_supported_fan_mode(climate::CLIMATE_FAN_LOW);
  traits.add_supported_fan_mode(climate::CLIMATE_FAN_MEDIUM);
  traits.add_supported_fan_mode(climate::CLIMATE_FAN_HIGH);

  // ESPHome climate temperatures are Celsius internally.
  // Home Assistant can display Fahrenheit according to user settings.
  traits.set_visual_min_temperature(16.0f);
  traits.set_visual_max_temperature(30.0f);
  traits.set_visual_temperature_step(1.0f);

  return traits;
}

void BC7215ACClimate::control(const climate::ClimateCall &call) {
  if (!this->ac_ || !this->is_paired()) {
    this->set_status_("Not paired. Press pair first.");
    return;
  }

  if (call.get_target_temperature().has_value()) {
    this->target_temperature = *call.get_target_temperature();
	this->ac_pressed_key_ = 0;
  }

  if (call.get_fan_mode().has_value()) {
    this->set_fan_mode_(*call.get_fan_mode());
	this->ac_pressed_key_ = 3;
  }

  if (call.get_mode().has_value()) {
    this->mode = *call.get_mode();
	this->ac_pressed_key_ = 2;
  }

  if (this->send_current_state_()) {
    this->set_status_("AC command sent.");
  } else {
    this->set_status_("AC command failed.");
  }

  this->publish_state();
}

void BC7215ACClimate::start_pairing() {
  if (!this->ac_) {
    this->set_status_("BC7215AC is not ready.");
    return;
  }

  this->mark_unpaired_();

  this->pair_state_ = PairState::PAIRING;
  this->pairing_ = true;
  this->match_index_ = 0;

  this->set_status_("Pairing: press Fan with 25°C/Cool mode");
  this->ac_->start_capture(1);
}

void BC7215ACClimate::stop_pairing() {
  if (this->ac_ && this->pairing_) {
    this->ac_->stop_capture();
  }
  this->pairing_ = false;
  this->pair_state_ = PairState::IDLE;
  this->set_status_("Pairing stopped.");
}

void BC7215ACClimate::builtin_or_next() {
  if (!this->ac_) {
    this->set_status_("BC7215AC is not ready.");
    return;
  }

  if (this->pairing_) {
    this->ac_->stop_capture();
    this->pairing_ = false;
    this->pair_state_ = PairState::IDLE;
  }

  if (this->is_paired()) {
    if (this->try_match_next_()) {
	  this->parsing_state_ = ParsingState::IDLE;
      this->save_pairing_();
      return;
    }
  }
  if (this->try_next_predefined_()) {
	this->parsing_state_ = ParsingState::STOP;
    this->save_pairing_();
    return;
  }
}

bool BC7215ACClimate::is_paired() const {
  return this->ac_ && this->ac_->init_ok;
}

uint8_t BC7215ACClimate::predefined_count() const {
  if (!this->ac_) {
    return 0;
  }
  return this->ac_->predefined_count();
}

void BC7215ACClimate::set_status_(const std::string &message) {
  this->status_message_ = message;
  ESP_LOGI(TAG, "%s", message.c_str());
}

void BC7215ACClimate::handle_pairing_loop_() {
  if (!this->pairing_ || !this->ac_) {
    return;
  }

  if (this->pair_state_ != PairState::PAIRING) {
    return;
  }

  if (!this->ac_->signal_captured()) {
    return;
  }

  this->ac_->stop_capture();
  this->pairing_ = false;
  this->pair_state_ = PairState::IDLE;

  if (this->ac_->init()) {
    this->match_index_ = 0;
	this->parsing_state_ = ParsingState::IDLE;
    this->save_pairing_();
    this->set_status_("AC Paired");
    this->publish_state();
  } else {
    this->mark_unpaired_();
    this->set_status_("Pair failed. Try Pair again or press Alt.");
  }
}

void BC7215ACClimate::handle_parsing_loop_() {
  if (this->pairing_ || !this->ac_ || this->ac_->is_busy() || !this->is_paired()) {
    return;
  }

  if (this->parsing_state_ == ParsingState::STOP) {
    return;
  }

  if (this->parsing_state_ == ParsingState::IDLE) {
    this->ac_->start_capture(0);
	this->parsing_state_ = ParsingState::PARSING;
    return;
  }

  if (!this->ac_->signal_captured()) {
    return;
  }

  this->ac_->stop_capture();
  this->last_base_ = *(reinterpret_cast<const bc7215DataMaxPkt_t*>(this->ac_->data_packet()));

  int t = -1;
  int m = -1;
  int f = -1;
  int p = -1;
  if (this->ac_->parse(t, m, f, p)) {
    this->mode = this->map_bc7215_mode_to_ha_(m);
	this->set_fan_mode_(this->map_bc7215_fan_to_ha_(f));
	this->target_temperature = library_temp_to_climate_temp_(t);
	if (p == 0) {
	  this->mode = climate::CLIMATE_MODE_OFF;
      this->action = climate::CLIMATE_ACTION_OFF;
	  this->power_on_ = false;
	  this->ac_->replace_base(last_base_);
	} else if (p == 2) {
	  if (this->mode != climate::CLIMATE_MODE_OFF) {
	    this->mode = climate::CLIMATE_MODE_OFF;
        this->action = climate::CLIMATE_ACTION_OFF;
		this->power_on_ = !this->power_on_;
		if (!this->power_on_) {
	  		this->ac_->replace_base(last_base_);
		}
	  }
	}
  }

  this->update_action_from_mode_();
  this->publish_state();
  this->parsing_state_ = ParsingState::IDLE;
}

bool BC7215ACClimate::load_pairing_() {
  StoredPairingData saved{};

  if (!this->pref_.load(&saved)) {
    ESP_LOGI(TAG, "No saved BC7215 AC pairing data.");
    this->mark_unpaired_();
    return false;
  }

  const bool configured_unit_celsius = this->library_unit_celsius_;

  this->library_unit_celsius_ = saved.library_unit_celsius;
  if (this->library_unit_celsius_) {
    this->ac_->set_celsius();
  } else {
    this->ac_->set_fahrenheit();
  }

  if (!this->ac_->init(saved.data, saved.format)) {
    ESP_LOGW(TAG, "Saved BC7215 AC pairing data exists, but init failed.");
    this->library_unit_celsius_ = configured_unit_celsius;
    if (this->library_unit_celsius_) {
      this->ac_->set_celsius();
    } else {
      this->ac_->set_fahrenheit();
    }
    this->mark_unpaired_();
    return false;
  }

  this->parsing_state_ = saved.parsing_state;
  this->match_index_ = 0;

  for (uint8_t i = 0; i < saved.match_index; ++i) {
    if (!this->ac_->match_next()) {
      ESP_LOGW(TAG, "Saved match index %u cannot be restored.", saved.match_index);
      this->match_index_ = 0;
	  this->ac_->init(saved.data, saved.format);	// use the first match
	  return true;
    }
    ++this->match_index_;
  }

  this->set_status_("Pairing restored from FLASH.");
  ESP_LOGI(TAG, "BC7215 AC pairing restored. match_index=%u, unit=%s",
           this->match_index_, this->library_unit_celsius_ ? "C" : "F");
  return true;
}

bool BC7215ACClimate::save_pairing_() {
  if (!this->ac_ || !this->ac_->init_ok) {
    ESP_LOGW(TAG, "Cannot save pairing data: AC library is not initialized.");
    return false;
  }

  const bc7215DataVarPkt_t *src_data = this->ac_->data_packet();
  const bc7215FormatPkt_t *src_format = this->ac_->format_packet();

  if (src_data == nullptr || src_format == nullptr) {
    ESP_LOGW(TAG, "Cannot save pairing data: data or format packet is null.");
    return false;
  }

  StoredPairingData saved{};

  const uint16_t data_bytes = static_cast<uint16_t>((src_data->bitLen + 7) / 8);
  if (data_bytes == 0 || data_bytes > sizeof(saved.data.data)) {
    ESP_LOGW(TAG, "Cannot save pairing data: invalid data size %u.", data_bytes);
    return false;
  }

  saved.data.bitLen = src_data->bitLen;
  std::memcpy(saved.data.data, src_data->data, data_bytes);
  saved.format = *src_format;
  saved.match_index = this->match_index_;
  saved.parsing_state = this->parsing_state_;
  saved.library_unit_celsius = this->library_unit_celsius_;

  if (!this->pref_.save(&saved)) {
    ESP_LOGW(TAG, "Failed to save BC7215 AC pairing data.");
    return false;
  }

  ESP_LOGI(TAG, "BC7215 AC pairing saved. match_index=%u, unit=%s",
           saved.match_index, saved.library_unit_celsius ? "C" : "F");
  return true;
}

bool BC7215ACClimate::try_next_predefined_() {
  const uint8_t count = this->predefined_count();

  this->predefined_number_++;
  this->match_index_ = 0;
  if (this->ac_->init_predefined(this->predefined_number_-1)) {
    std::string msg = "Use built-in Protocol ";
    msg += std::to_string(this->predefined_number_);
    this->set_status_(msg);
    this->send_test_command_();
    this->publish_state();
    return true;
  }
  if (this->predefined_number_ > count)
  {
    this->predefined_number_ = 0;
  }
  this->mark_unpaired_();
  return false;
}

bool BC7215ACClimate::try_match_next_() {
  bool result;
  result = this->ac_->match_next();
  if (result) {
  	this->match_index_++;
      std::string msg = "Alt matched protocol ";
      msg += std::to_string(this->match_index_);
      this->set_status_(msg);
      this->send_test_command_();
      this->publish_state();
  } else {
    this->match_index_ = 0;
  }
  return result;
}

bool BC7215ACClimate::send_current_state_() {
  if (!this->ac_ || !this->is_paired()) {
    return false;
  }

  this->ac_->stop_capture();
  this->parsing_state_ = ParsingState::IDLE;

  if (this->mode == climate::CLIMATE_MODE_OFF) {
    const auto *pkt = this->ac_->off();
    this->action = climate::CLIMATE_ACTION_OFF;
	this->power_on_ = false;
    return pkt != nullptr;
  }

  const int temp = this->climate_temp_to_library_temp_(this->target_temperature);
  const int mode = this->map_mode_to_bc7215_(this->mode);
  const int fan = this->map_fan_to_bc7215_();

  if (!this->power_on_)
  {
  	this->ac_->on();
	this->power_on_ = true;
  }
  const auto *pkt = this->ac_->set_to(temp, mode, fan, this->ac_pressed_key_);
  this->update_action_from_mode_();
  return pkt != nullptr;
}

bool BC7215ACClimate::send_test_command_() {
  if (!this->ac_ || !this->is_paired()) {
    return false;
  }

  // Test with a safe common command: Cool 25C, fan auto.
  const int temp = this->climate_temp_to_library_temp_(25.0f);
  const auto *pkt = this->ac_->set_to(temp, 1, 0);

  this->mode = climate::CLIMATE_MODE_COOL;
  this->target_temperature = 25.0f;
  this->set_fan_mode_(climate::CLIMATE_FAN_AUTO);
  this->action = climate::CLIMATE_ACTION_COOLING;

  return pkt != nullptr;
}

void BC7215ACClimate::mark_unpaired_() {
  if (this->ac_) {
    this->ac_->init_ok = false;
  }

  this->mode = climate::CLIMATE_MODE_OFF;
  this->target_temperature = 25.0f;
  this->set_fan_mode_(climate::CLIMATE_FAN_AUTO);
  this->action = climate::CLIMATE_ACTION_OFF;

  this->pairing_ = false;
  this->pair_state_ = PairState::IDLE;
  this->parsing_state_ = ParsingState::STOP;
  this->match_index_ = 0;
  this->set_status_("Not Paired. Prepare remote to 25°C/Cool");

  this->publish_state();
}

int BC7215ACClimate::map_mode_to_bc7215_(climate::ClimateMode mode) const {
  // BC7215 AC example table:
  // 0 Auto, 1 Cool, 2 Heat, 3 Dry, 4 Fan
  switch (mode) {
    case climate::CLIMATE_MODE_AUTO:
      return 0;
    case climate::CLIMATE_MODE_COOL:
      return 1;
    case climate::CLIMATE_MODE_HEAT:
      return 2;
    case climate::CLIMATE_MODE_DRY:
      return 3;
    case climate::CLIMATE_MODE_FAN_ONLY:
      return 4;
    default:
      return 0;
  }
}

int BC7215ACClimate::map_fan_to_bc7215_() const {
  // BC7215 AC example table:
  // 0 Auto, 1 Low, 2 Medium, 3 High
  if (!this->fan_mode.has_value()) {
    return 0;
  }

  switch (*this->fan_mode) {
    case climate::CLIMATE_FAN_AUTO:
      return 0;
    case climate::CLIMATE_FAN_LOW:
      return 1;
    case climate::CLIMATE_FAN_MEDIUM:
      return 2;
    case climate::CLIMATE_FAN_HIGH:
      return 3;
    default:
      return 0;
  }
}

climate::ClimateMode BC7215ACClimate::map_bc7215_mode_to_ha_(int m) const {

  switch (m) {
    case 0:
      return climate::CLIMATE_MODE_AUTO;

    case 1:
      return climate::CLIMATE_MODE_COOL;

    case 2:
      return climate::CLIMATE_MODE_HEAT;

    case 3:
      return climate::CLIMATE_MODE_DRY;

    case 4:
      return climate::CLIMATE_MODE_FAN_ONLY;

    default:
      return this->mode;
  }
}

climate::ClimateFanMode BC7215ACClimate::map_bc7215_fan_to_ha_(int f) const {
  // BC7215 AC example table:
  // 0 Auto, 1 Low, 2 Medium, 3 High
  switch (f) {
    case 0:
      return climate::CLIMATE_FAN_AUTO;

    case 1:
      return climate::CLIMATE_FAN_LOW;

    case 2:
      return climate::CLIMATE_FAN_MEDIUM;

    case 3:
      return climate::CLIMATE_FAN_HIGH;

    default:
      return *this->fan_mode;
  }
}

int BC7215ACClimate::climate_temp_to_library_temp_(float temp_c) const {
  if (std::isnan(temp_c)) {
    temp_c = 25.0f;
  }

  if (this->library_unit_celsius_) {
    return static_cast<int>(std::lround(temp_c));
  }

  // ESPHome climate uses Celsius internally. Convert to Fahrenheit only for
  // the BC7215 AC library if the library is using Fahrenheit protocol tables.
  return static_cast<int>(std::lround(temp_c * 9.0f / 5.0f + 32.0f));
}

float BC7215ACClimate::library_temp_to_climate_temp_(int t) const {
  if (this->library_unit_celsius_) {
    if ((t < 16) || (t > 30)) {
	  return this->target_temperature;
	}
    return static_cast<float>(t);
  }

  if ((t < 60) || (t > 88)) {
	return this->target_temperature;
  }
  return (static_cast<float>(t) - 32.0f) * 5.0f / 9.0f;
}

void BC7215ACClimate::update_action_from_mode_() {
  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      this->action = climate::CLIMATE_ACTION_COOLING;
      break;
    case climate::CLIMATE_MODE_HEAT:
      this->action = climate::CLIMATE_ACTION_HEATING;
      break;
    case climate::CLIMATE_MODE_DRY:
      this->action = climate::CLIMATE_ACTION_DRYING;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      this->action = climate::CLIMATE_ACTION_FAN;
      break;
    case climate::CLIMATE_MODE_AUTO:
      this->action = climate::CLIMATE_ACTION_IDLE;
      break;
    default:
      this->action = climate::CLIMATE_ACTION_IDLE;
      break;
  }
}

}  // namespace bc7215_ac
}  // namespace esphome
