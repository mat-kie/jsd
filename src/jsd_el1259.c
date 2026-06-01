#include "jsd/jsd_el1259.h"

#include <assert.h>

#include "jsd/jsd_sdo.h"

// Bit positions within the per-channel PDO control/status bytes (see the ESI,
// docs/esi/Beckhoff EL1xxx.xml). Note: "Enable manual operation" is NOT a PDO
// control bit -- it is the SDO 0x80n1:02 written once during PO2SO config (see
// JSD_EL1259_ENABLE_MANUAL_OP_SUBIND below). Ctrl-byte bits 3-7 are reserved.
#define JSD_EL1259_INPUT_STATE_BIT (0x01u << 0)       // 0x60n1:09 in status byte
#define JSD_EL1259_MANUAL_OUTPUT_STATE_BIT (0x01u << 1)  // 0x70n1:02 in ctrl byte

// Valid digital-IO PDO assignment. The EL1259 requires BOTH PDO families on
// each SyncManager (mirroring its default image); mapping one family alone is
// rejected (AL status 0x0024/0x0025). Each SM is assigned two families, grouped
// in this order:
//   SM3 inputs : 8x "MTO Inputs"    (0x1A00+ch,   output status, 4B)  companion
//              + 8x "MTI Inputs 1x" (0x1A0B+4*ch,  digital input,12B) <- DI level
//   SM2 outputs: 8x "MTO Outputs 1x"(0x1603+4*ch,  manual output,12B) <- DO level
//              + 8x "MTI Outputs"   (0x1620+ch,    input control, 4B) companion
#define JSD_EL1259_TXPDO_ASSIGN_SDO (uint16_t)0x1C13  // inputs  (SM3)
#define JSD_EL1259_RXPDO_ASSIGN_SDO (uint16_t)0x1C12  // outputs (SM2)
#define JSD_EL1259_MTO_INPUTS_BASE (uint16_t)0x1A00   // "MTO Inputs"     Ch.1
#define JSD_EL1259_MTI_INPUTS_BASE (uint16_t)0x1A0B   // "MTI Inputs 1x"  Ch.1
#define JSD_EL1259_MTO_OUTPUTS_BASE (uint16_t)0x1603  // "MTO Outputs 1x" Ch.1
#define JSD_EL1259_MTI_OUTPUTS_BASE (uint16_t)0x1620  // "MTI Outputs"    Ch.1
#define JSD_EL1259_TS_PDO_STRIDE (uint16_t)0x0004     // stride for the 1x/Nx PDOs
#define JSD_EL1259_CH_PDO_STRIDE (uint16_t)0x0001     // stride for per-channel companions

// Per-channel "MTO Settings" object (0x8001 + 0x10*ch), subindex 0x02 is
// "Enable manual operation".
#define JSD_EL1259_MTO_SETTINGS_BASE (uint16_t)0x8001
#define JSD_EL1259_MTO_SETTINGS_STRIDE (uint16_t)0x0010
#define JSD_EL1259_ENABLE_MANUAL_OP_SUBIND (uint8_t)0x02

/****************************************************
 * Public functions
 ****************************************************/

const jsd_el1259_state_t* jsd_el1259_get_state(jsd_t* self, uint16_t slave_id) {
  assert(self);
  assert(jsd_el1259_product_code_is_compatible(
      self->ecx_context.slavelist[slave_id].eep_id));

  return &self->slave_states[slave_id].el1259;
}

void jsd_el1259_read(jsd_t* self, uint16_t slave_id) {
  assert(self);
  assert(jsd_el1259_product_code_is_compatible(
      self->ecx_context.slavelist[slave_id].eep_id));

  jsd_el1259_state_t* state = &self->slave_states[slave_id].el1259;

  const jsd_el1259_txpdo_t* txpdo =
      (jsd_el1259_txpdo_t*)self->ecx_context.slavelist[slave_id].inputs;

  for (int ch = 0; ch < JSD_EL1259_NUM_DI_CHANNELS; ++ch) {
    // The digital input level lives in the "MTI Inputs" family status byte.
    state->input[ch] = (uint8_t)((txpdo->mti_input[ch].status &
                                  JSD_EL1259_INPUT_STATE_BIT) != 0);
  }
}

void jsd_el1259_process(jsd_t* self, uint16_t slave_id) {
  assert(self);
  assert(jsd_el1259_product_code_is_compatible(
      self->ecx_context.slavelist[slave_id].eep_id));

  jsd_el1259_state_t* state = &self->slave_states[slave_id].el1259;

  jsd_el1259_rxpdo_t* rxpdo =
      (jsd_el1259_rxpdo_t*)self->ecx_context.slavelist[slave_id].outputs;

  for (int ch = 0; ch < JSD_EL1259_NUM_DO_CHANNELS; ++ch) {
    // Drive the physical output via the "Manual output state" bit (0x70n1:02).
    // Manual mode itself is enabled once via the SDO 0x80n1:02 during PO2SO
    // config -- it is not a per-cycle PDO control bit.
    uint8_t ctrl = 0;
    if (state->output[ch] > 0) {
      ctrl |= JSD_EL1259_MANUAL_OUTPUT_STATE_BIT;
    }
    // The manual output level lives in the "MTO Outputs" family ctrl byte.
    rxpdo->mto_output[ch].ctrl = ctrl;
  }
}

void jsd_el1259_write_single_channel(jsd_t* self, uint16_t slave_id,
                                     uint8_t channel, uint8_t output) {
  assert(self);
  assert(jsd_el1259_product_code_is_compatible(
      self->ecx_context.slavelist[slave_id].eep_id));
  assert(channel < JSD_EL1259_NUM_DO_CHANNELS);

  self->slave_states[slave_id].el1259.output[channel] = output;
}

void jsd_el1259_write_all_channels(jsd_t* self, uint16_t slave_id,
                                   uint8_t output[JSD_EL1259_NUM_DO_CHANNELS]) {
  for (int ch = 0; ch < JSD_EL1259_NUM_DO_CHANNELS; ++ch) {
    jsd_el1259_write_single_channel(self, slave_id, (uint8_t)ch, output[ch]);
  }
}

/****************************************************
 * Private functions
 ****************************************************/

bool jsd_el1259_init(jsd_t* self, uint16_t slave_id) {
  assert(self);
  assert(jsd_el1259_product_code_is_compatible(
      self->ecx_context.slavelist[slave_id].eep_id));
  assert(self->ecx_context.slavelist[slave_id].eep_man ==
         JSD_BECKHOFF_VENDOR_ID);

  ec_slavet* slaves = self->ecx_context.slavelist;
  ec_slavet* slave  = &slaves[slave_id];

  slave->PO2SOconfigx = jsd_el1259_PO2SO_config;

  return true;
}

// Append `num_channels` PDOs (pdo_base + stride*ch) to a SyncManager assignment
// object starting at subindex `start+1`, written subindex-by-subindex (the
// EL125x family is sensitive to assignment order). Returns the next free
// subindex, or 0 on SDO failure.
static uint8_t jsd_el1259_append_pdos(ecx_contextt* ecx_context,
                                      uint16_t slave_id, uint16_t assign_sdo,
                                      uint8_t start, uint16_t pdo_base,
                                      uint16_t stride, uint8_t num_channels) {
  for (uint8_t ch = 0; ch < num_channels; ++ch) {
    uint16_t pdo_index = (uint16_t)(pdo_base + stride * ch);
    if (!jsd_sdo_set_param_blocking(ecx_context, slave_id, assign_sdo,
                                    (uint8_t)(start + 1 + ch), JSD_SDO_DATA_U16,
                                    &pdo_index)) {
      return 0;
    }
  }
  return (uint8_t)(start + num_channels);
}

// Reassign one SyncManager to TWO PDO families (family A then family B), as the
// EL1259 requires both per SM. Clears the assignment, appends both families,
// then writes the final entry count.
static int jsd_el1259_assign_sm(ecx_contextt* ecx_context, uint16_t slave_id,
                                uint16_t assign_sdo, uint16_t base_a,
                                uint16_t stride_a, uint16_t base_b,
                                uint16_t stride_b, uint8_t num_channels) {
  uint8_t zero = 0;
  if (!jsd_sdo_set_param_blocking(ecx_context, slave_id, assign_sdo, 0x00,
                                  JSD_SDO_DATA_U8, &zero)) {
    return 0;
  }
  uint8_t n = jsd_el1259_append_pdos(ecx_context, slave_id, assign_sdo, 0,
                                     base_a, stride_a, num_channels);
  if (n == 0) {
    return 0;
  }
  n = jsd_el1259_append_pdos(ecx_context, slave_id, assign_sdo, n, base_b,
                             stride_b, num_channels);
  if (n == 0) {
    return 0;
  }
  if (!jsd_sdo_set_param_blocking(ecx_context, slave_id, assign_sdo, 0x00,
                                  JSD_SDO_DATA_U8, &n)) {
    return 0;
  }
  return 1;
}

int jsd_el1259_PO2SO_config(ecx_contextt* ecx_context, uint16_t slave_id) {
  assert(ecx_context);
  assert(jsd_el1259_product_code_is_compatible(
      ecx_context->slavelist[slave_id].eep_id));

  // Since this function prototype is forced by SOEM, we have embedded a
  // reference to jsd.slave_configs within the ecx_context and extract it here.
  jsd_slave_config_t* slave_configs =
      (jsd_slave_config_t*)ecx_context->userdata;
  jsd_slave_config_t* config = &slave_configs[slave_id];

  MSG("Configuring slave no: %u, SII inferred name: %s", slave_id,
      ecx_context->slavelist[slave_id].name);
  MSG("\t Configured name: %s", config->name);

  // Reset to factory default before (re)assigning the process data.
  uint32_t reset_word = JSD_BECKHOFF_RESET_WORD;
  if (!jsd_sdo_set_param_blocking(ecx_context, slave_id, JSD_BECKHOFF_RESET_SDO,
                                  JSD_BECKHOFF_RESET_SUBIND, JSD_SDO_DATA_U32,
                                  &reset_word)) {
    return 0;
  }

  // Replace the default (heavy 10x multi-timestamp) image with the smaller "1x"
  // variant, but keep BOTH PDO families per SyncManager as the device requires:
  //   SM3 = MTO Inputs (status) + MTI Inputs 1x (digital input level)
  //   SM2 = MTO Outputs 1x (manual output level) + MTI Outputs (input control)
  if (!jsd_el1259_assign_sm(ecx_context, slave_id, JSD_EL1259_TXPDO_ASSIGN_SDO,
                            JSD_EL1259_MTO_INPUTS_BASE, JSD_EL1259_CH_PDO_STRIDE,
                            JSD_EL1259_MTI_INPUTS_BASE, JSD_EL1259_TS_PDO_STRIDE,
                            JSD_EL1259_NUM_DI_CHANNELS)) {
    ERROR("EL1259 slave %u: failed to assign input PDOs", slave_id);
    return 0;
  }
  if (!jsd_el1259_assign_sm(ecx_context, slave_id, JSD_EL1259_RXPDO_ASSIGN_SDO,
                            JSD_EL1259_MTO_OUTPUTS_BASE, JSD_EL1259_TS_PDO_STRIDE,
                            JSD_EL1259_MTI_OUTPUTS_BASE, JSD_EL1259_CH_PDO_STRIDE,
                            JSD_EL1259_NUM_DO_CHANNELS)) {
    ERROR("EL1259 slave %u: failed to assign output PDOs", slave_id);
    return 0;
  }

  // Enable manual (direct level) operation for every output channel so the
  // "Manual output state" bit in the output PDO drives the physical output.
  uint8_t enable = 1;
  for (uint8_t ch = 0; ch < JSD_EL1259_NUM_DO_CHANNELS; ++ch) {
    uint16_t settings_index =
        (uint16_t)(JSD_EL1259_MTO_SETTINGS_BASE +
                   JSD_EL1259_MTO_SETTINGS_STRIDE * ch);
    if (!jsd_sdo_set_param_blocking(ecx_context, slave_id, settings_index,
                                    JSD_EL1259_ENABLE_MANUAL_OP_SUBIND,
                                    JSD_SDO_DATA_U8, &enable)) {
      ERROR("EL1259 slave %u: failed to enable manual operation on ch %u",
            slave_id, ch + 1);
      return 0;
    }
  }

  config->PO2SO_success = true;
  return 1;
}

bool jsd_el1259_product_code_is_compatible(uint32_t product_code) {
  return product_code == JSD_EL1259_PRODUCT_CODE;
}
