/*
 *  Copyright © 2017-2022 Wellington Wallace
 *
 *  This file is part of EasyEffects.
 *
 *  EasyEffects is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  EasyEffects is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with EasyEffects.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef PWBASEPLUGIN_H
#define PWBASEPLUGIN_H

#include <giomm.h>
#include <pipewire/filter.h>
// #include <spa/param/latency-utils.h> // unavailable on Ubuntu 21.04 >:(
#include <mutex>
#include <ranges>
#include <span>

#include "PwPipelineManager.h"
#include "Utils.h"

class PwPluginBase {
 public:
  PwPluginBase(std::string tag,
             std::string plugin_name,
             PwPipelineManager* pipe_manager,
             const bool& enable_probe = false);
  PwPluginBase(const PwPluginBase&) = delete;
  auto operator=(const PwPluginBase&) -> PwPluginBase& = delete;
  PwPluginBase(const PwPluginBase&&) = delete;
  auto operator=(const PwPluginBase&&) -> PwPluginBase& = delete;
  virtual ~PwPluginBase();

  struct data;

  struct port {
    struct data* data;
  };

  struct data {
    struct port* in_left = nullptr;
    struct port* in_right = nullptr;

    struct port* out_left = nullptr;
    struct port* out_right = nullptr;

    struct port* probe_left = nullptr;
    struct port* probe_right = nullptr;

    PwPluginBase* pb = nullptr;
  };

  const std::string log_tag;

  std::string name;

  pw_filter* filter = nullptr;

  bool enable_probe = false;

  uint n_samples = 0U;

  uint rate = 0U;

  float sample_duration = 0.0F;

  bool bypass = false;

  bool connected_to_pw = false;

  bool post_messages = false;

  [[nodiscard]] auto get_node_id() const -> uint;

  void set_active(const bool& state) const;

  auto connect_to_pw() -> bool;

  void disconnect_from_pw();

  virtual void setup();

  virtual void process(float* left_in,
                       float* right_in,
                       float* left_out,
                       float* right_out,
                       size_t length);

  virtual void process(float* left_in,
                       float* right_in,
                       float* left_out,
                       float* right_out,
                       float* probe_left,
                       float* probe_right,
                       size_t length);

  sigc::signal<void(const float&, const float&)> input_level;
  sigc::signal<void(const float&, const float&)> output_level;

 protected:
  std::mutex data_mutex;

  Glib::RefPtr<Gio::Settings> settings;

  PwPipelineManager* pm = nullptr;

  spa_hook listener{};

  data pf_data = {};

  float input_gain = 1.0F;
  float output_gain = 1.0F;

  float notification_time_window = 1.0F / 20.0F;  // seconds
  float notification_dt = 0.0F;

  void initialize_listener();

  void notify();

  void get_peaks(const float* left_in,
                 const float* right_in,
                 float* left_out,
                 float* right_out,
                 size_t length);

 private:
  uint node_id = 0U;

  float input_peak_left = util::minimum_linear_level, input_peak_right = util::minimum_linear_level;
  float output_peak_left = util::minimum_linear_level, output_peak_right = util::minimum_linear_level;
};

#endif
