#include <algorithm>

#include "foxess_s.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace foxess_s {

void publish_sensor_state(sensor::Sensor *sensor, int32_t val, float unit) {
  if (sensor == nullptr)
    return;
  float value = val * unit;
  sensor->publish_state(value);
};

int16_t encode_int16(uint8_t msb, uint8_t lsb) { return static_cast<uint16_t>(msb << 8 | lsb); }

void FoxessS::setup() {
  ESP_LOGVV("FoxessS::setup", "start");

  this->flow_control_pin_->setup();
  this->millis_lastmessage_ = millis();
}

void FoxessS::update() {
  ESP_LOGVV("FoxessS::update", "start");
  if (millis() - this->millis_lastmessage_ >= INVERTER_TIMEOUT) {
    if (this->inverter_mode_ != 0) {
      this->set_inverter_mode(0);  // OFFLINE
      publish_sensor_state(this->generation_power_, 0, 1);
      publish_sensor_state(this->grid_power_, 0, NAN);
      publish_sensor_state(this->loads_power_, 0, NAN);

      publish_sensor_state(this->boost_temp_, 0, NAN);
      publish_sensor_state(this->ambient_temp_, 0, NAN);
      publish_sensor_state(this->inverter_temp_, 0, NAN);

      PUBLISH_ZERO_GRID//(0, 1, 2)
      PUBLISH_ZERO_PV//(0, 1, 2)
    }
  }

  size_t avail = this->available();
  if (avail == 0) {
    return;
  }

  std::array<uint8_t, BUFFER_SIZE> local_buff{};
  this->read_array(local_buff.data(), std::min<size_t>(avail, BUFFER_SIZE));

  for (size_t i = 0; i < avail; i++) {
    this->input_buffer[this->buffer_end] = local_buff[i];
    optional<bool> is_valid = this->check_msg();

    if (!is_valid.has_value()) {
      // Message is still valid, continue reading
      this->buffer_end++;
    } else if (*is_valid) {
      // Message finished and valid
      this->status_clear_warning();
      this->parse_message();
      this->buffer_end = 0;
      this->millis_lastmessage_ = millis();
    } else {
      // Message is invalid, clear buffer
      this->buffer_end = 0;
    }
  }
}

// Return true if message is still valid
// Return false if message is invalid (buffer should be cleared)
// Return empty if message is complete and valid
optional<bool> FoxessS::check_msg() {
  size_t idx = this->buffer_end;

  // Check if Header is valid
  if (idx <= 2) {
    if (this->input_buffer[idx] == MSG_HEADER[idx]) {
      return {};
    } else {
      ESP_LOGVV("FoxessS::check_msg", "Start of message incorrect: 0x%x, 0x%x, 0x%x", this->input_buffer[0],
                this->input_buffer[1], this->input_buffer[2]);
      return false;
    }
  }

  // Check if msg_len available
  if (idx < 9) {
    return {};
  }

  // Check if buffer is full
  if (idx + 1 == BUFFER_SIZE) {
    ESP_LOGE("FoxessS::check_msg", "Buffer full");
    this->status_set_warning();
    return false;
  }

  // Check if message length is correct
  uint16_t msg_len = encode_uint16(this->input_buffer[7], this->input_buffer[8]) + 13;
  if (idx + 1 < msg_len) {
    ESP_LOGVV("FoxessS::check_msg", "Message not ready, size: %d, idx: %d", msg_len, idx);
    return {};
  }

  // Check if footer is correct
  if (this->input_buffer[idx - 1] != MSG_FOOTER[0] || this->input_buffer[idx] != MSG_FOOTER[1]) {
    ESP_LOGE("FoxessS::check_msg", "Message footer incorrect [..., 0x%x, 0x%x]", this->input_buffer[idx - 1],
             this->input_buffer[idx]);
    this->status_set_warning();
    return false;
  }

  // Check CRC
  uint16_t received_crc = crc16(&this->input_buffer[2],  // Data start after header size 2
                                msg_len - 6);            // size -2 HEAD -2 FOOT -2 CHECKSUM
  uint16_t calc_crc = encode_uint16(this->input_buffer[idx - 2], this->input_buffer[idx - 3]);
  if (received_crc != calc_crc) {
    ESP_LOGE("FoxessS::check_msg", "Checksum mismatch, calc: 0x%x, message: 0x%x", calc_crc, received_crc);
    this->status_set_warning();
    return false;
  }

  return true;
}

void FoxessS::parse_message() {
  ESP_LOGVV("FoxessS::parse_message", "start");
  this->millis_lastmessage_ = millis();

  if (this->buffer_end + 1 != 251) {
    ESP_LOGW("FoxessS::parse_message", "Unexpected msg length, length: %d", this->buffer_end + 1);
    this->status_set_warning();
  }

  auto &msg = this->input_buffer;
  publish_sensor_state(this->grid_power_, encode_int16(msg[9], msg[10]), 1);
  publish_sensor_state(this->generation_power_, encode_uint16(msg[11], msg[12]), 1);
  publish_sensor_state(this->loads_power_, encode_int16(msg[13], msg[14]), 1);

  publish_sensor_state(this->grid.voltage_sensor_, encode_uint16(msg[21], msg[22]), 0.1);
  publish_sensor_state(this->grid.current_sensor_, encode_uint16(msg[15], msg[16]), 0.1);
  publish_sensor_state(this->grid.frequency_sensor_, encode_uint16(msg[25], msg[26]), 0.01);
  publish_sensor_state(this->grid.active_power_sensor_, encode_uint16(msg[21], msg[22]), 1);

  volt = encode_uint16(msg[45], msg[46]);
  amps = encode_uint16(msg[47], msg[48]);
  publish_sensor_state(this->pv.voltage_sensor_, volt, 0.1);
  publish_sensor_state(this->pv.current_sensor_, amps, 0.1);
  publish_sensor_state(this->pv.active_power_sensor_, volt * amps, 0.01);

  publish_sensor_state(this->boost_temp_, encode_int16(msg[63], msg[64]), 1);
  publish_sensor_state(this->inverter_temp_, encode_int16(msg[65], msg[66]), 1);
  publish_sensor_state(this->ambient_temp_, encode_int16(msg[67], msg[68]), 1);

  publish_sensor_state(this->energy_production_day_, encode_uint16(msg[69], msg[70]), 0.1);

  publish_sensor_state(this->total_energy_production_, encode_uint32(msg[71], msg[72], msg[73], msg[74]), 0.1);

  if (!std::all_of(this->input_buffer.begin() + 125, this->input_buffer.begin() + 157, [](int i) { return i == 0; })) {
    this->set_inverter_mode(2);  // ERROR
    return;
  }

  this->set_inverter_mode(1);  // ONLINE
}

void FoxessS::set_inverter_mode(uint32_t mode) {
  this->inverter_mode_ = mode;
  if (this->inverter_status_ != nullptr)
    this->inverter_status_->publish_state(mode);
}

}  // namespace foxess_s
}   //namespace esphome
