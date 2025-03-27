#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/uart/uart.h"

#define SENSOR_SETTER(X) \
  void set_##X##_sensor(sensor::Sensor *sensor) { this->X##_ = sensor; }

#define GRID_SENSOR_SETTER(X) \
  void set_grid_##X##_sensor(uint8_t grid, sensor::Sensor *X##_sensor) { \
    this->grid.X##_sensor_ = X##_sensor; \
  }

#define PV_SENSOR_SETTER(X) \
  void set_pv_##X##_sensor(uint8_t pv, sensor::Sensor *X##_sensor) { this->pv.X##_sensor_ = X##_sensor; }

#define PUBLISH_ZERO_GRID(...) \
  { \
      publish_sensor_state(this->grid.voltage_sensor_, 0, 1); \
      publish_sensor_state(this->grid.current_sensor_, 0, 1); \
      publish_sensor_state(this->grid.frequency_sensor_, 0, NAN); \
      publish_sensor_state(this->grid.active_power_sensor_, 0, 1); \
  }
#define PUBLISH_ZERO_PV(...) \
  { \
      publish_sensor_state(this->pv.voltage_sensor_, 0, 1); \
      publish_sensor_state(this->pv.current_sensor_, 0, 1); \
      publish_sensor_state(this->pv.active_power_sensor_, 0, 1); \
  }

namespace esphome {
namespace foxess_s {

static const long INVERTER_TIMEOUT = 300000;  // ms
static const size_t BUFFER_SIZE = 256;

static const std::array<uint8_t, 3> MSG_HEADER = {0x7E, 0x7E, 0x02};
static const std::array<uint8_t, 2> MSG_FOOTER = {0xE7, 0xE7};

class FoxessS : public PollingComponent, public uart::UARTDevice {
 public:
  void setup() override;
  void update() override;

  SENSOR_SETTER(loads_power)
  SENSOR_SETTER(grid_power)
  SENSOR_SETTER(generation_power)
  SENSOR_SETTER(boost_temp)
  SENSOR_SETTER(inverter_temp)
  SENSOR_SETTER(ambient_temp)
  SENSOR_SETTER(energy_production_day)
  SENSOR_SETTER(total_energy_production)
  SENSOR_SETTER(inverter_status)

  GRID_SENSOR_SETTER(voltage)
  GRID_SENSOR_SETTER(current)
  GRID_SENSOR_SETTER(active_power)
  GRID_SENSOR_SETTER(frequency)

  PV_SENSOR_SETTER(voltage)
  PV_SENSOR_SETTER(current)
  PV_SENSOR_SETTER(active_power)

  void set_fc_pin(GPIOPin *flow_control_pin) { this->flow_control_pin_ = flow_control_pin; }

 protected:
  void parse_message();
  void set_inverter_mode(uint32_t mode);
  optional<bool> check_msg();

  GPIOPin *flow_control_pin_{nullptr};
  uint32_t millis_lastmessage_{0};
  std::array<uint8_t, BUFFER_SIZE> input_buffer{};
  size_t buffer_end{0};

  uint32_t inverter_mode_{99};
  sensor::Sensor *inverter_status_{nullptr};  // 0=Offline, 1=Online, 2=Error, 99=Waiting for response...

  struct SolarGrid {
    sensor::Sensor *voltage_sensor_{nullptr};
    sensor::Sensor *current_sensor_{nullptr};
    sensor::Sensor *active_power_sensor_{nullptr};
    sensor::Sensor *frequency_sensor_{nullptr};
  } grid;

  struct SolarPV {
    sensor::Sensor *voltage_sensor_{nullptr};
    sensor::Sensor *current_sensor_{nullptr};
    sensor::Sensor *active_power_sensor_{nullptr};
  } pv;

  sensor::Sensor *loads_power_{nullptr};
  sensor::Sensor *grid_power_{nullptr};
  sensor::Sensor *generation_power_{nullptr};

  sensor::Sensor *boost_temp_{nullptr};
  sensor::Sensor *inverter_temp_{nullptr};
  sensor::Sensor *ambient_temp_{nullptr};
  sensor::Sensor *energy_production_day_{nullptr};
  sensor::Sensor *total_energy_production_{nullptr};
};

}  // namespace foxess_solar
}  // namespace esphome
