#include <assert.h>
#include <string.h>

#include "jsd/jsd_el1259_pub.h"
#include "jsd/jsd_el1259_types.h"
#include "jsd_test_utils.h"

extern bool  quit;
extern FILE* file;
uint8_t      slave_id;

void telemetry_header() {
  if (!file) {
    return;
  }
  for (int i = 0; i < JSD_EL1259_NUM_DI_CHANNELS; ++i) {
    fprintf(file, "EL1259_input_ch%d, ", i);
  }
  for (int i = 0; i < JSD_EL1259_NUM_DO_CHANNELS; ++i) {
    fprintf(file, "EL1259_output_ch%d, ", i);
  }
  fprintf(file, "\n");
}

void telemetry_data(void* self) {
  assert(self);

  if (!file) {
    return;
  }

  single_device_server_t*   sds   = (single_device_server_t*)self;
  const jsd_el1259_state_t* state = jsd_el1259_get_state(sds->jsd, slave_id);

  for (int i = 0; i < JSD_EL1259_NUM_DI_CHANNELS; ++i) {
    fprintf(file, "%d,", state->input[i]);
  }
  for (int i = 0; i < JSD_EL1259_NUM_DO_CHANNELS; ++i) {
    fprintf(file, "%d,", state->output[i]);
  }
  fprintf(file, "\n");
  fflush(file);
}

void print_info(void* self) {
  assert(self);

  single_device_server_t*   sds   = (single_device_server_t*)self;
  const jsd_el1259_state_t* state = jsd_el1259_get_state(sds->jsd, slave_id);
  MSG("Inputs:  %d %d %d %d %d %d %d %d", state->input[0], state->input[1],
      state->input[2], state->input[3], state->input[4], state->input[5],
      state->input[6], state->input[7]);
  MSG("Outputs: %d %d %d %d %d %d %d %d", state->output[0], state->output[1],
      state->output[2], state->output[3], state->output[4], state->output[5],
      state->output[6], state->output[7]);
}

void extract_data(void* self) {
  assert(self);

  single_device_server_t* sds = (single_device_server_t*)self;
  jsd_el1259_read(sds->jsd, slave_id);
}

// Drive a walking-bit pattern across the 8 outputs so the test exercises the
// output PDO each cycle.
void command(void* self) {
  assert(self);

  static uint8_t step = 0;
  single_device_server_t* sds = (single_device_server_t*)self;

  uint8_t output[JSD_EL1259_NUM_DO_CHANNELS] = {0};
  output[step % JSD_EL1259_NUM_DO_CHANNELS] = 1;
  ++step;

  jsd_el1259_write_all_channels(sds->jsd, slave_id, output);
  jsd_el1259_process(sds->jsd, slave_id);
}

int main(int argc, char* argv[]) {
  if (argc != 4) {
    ERROR("Expecting exactly 3 arguments");
    MSG("Usage: jsd_el1259_test <ifname> <el1259_slave_index> <loop_freq_hz>");
    MSG("Example: $ jsd_el1259_test eth0 2 1000");
    return 0;
  }

  char* ifname          = strdup(argv[1]);
  slave_id              = atoi(argv[2]);
  uint32_t loop_freq_hz = atoi(argv[3]);
  MSG("Configuring device %s, using slave %d", ifname, slave_id);
  MSG("Using frequency of %i hz", loop_freq_hz);

  single_device_server_t sds;

  sds_set_telemetry_header_callback(&sds, telemetry_header);
  sds_set_telemetry_data_callback(&sds, telemetry_data);
  sds_set_print_info_callback(&sds, print_info);
  sds_set_extract_data_callback(&sds, extract_data);
  sds_set_command_callback(&sds, command);

  sds_setup(&sds, loop_freq_hz);

  // Set device configuration here.
  jsd_slave_config_t my_config = {0};

  snprintf(my_config.name, JSD_NAME_LEN, "unicorn");
  my_config.configuration_active = true;
  my_config.driver_type          = JSD_DRIVER_TYPE_EL1259;

  // EL1259 requires DC SYNC0 active to reach SAFE_OP/OP. Derive the SYNC0 cycle
  // from the test loop frequency (cycle_ns = 1e9 / loop_freq_hz).
  my_config.dc_sync0_enable   = true;
  my_config.dc_sync0_cycle_ns = (uint32_t)(1.0e9 / (double)loop_freq_hz);
  my_config.dc_sync0_shift_ns = 0;

  jsd_set_slave_config(sds.jsd, slave_id, my_config);

  sds_run(&sds, ifname, "/tmp/jsd_el1259.csv");

  free(ifname);

  return 0;
}
