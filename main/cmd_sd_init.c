/*
 * SDMMC Explicit Initialization Command
 * Forces proper SDMMC peripheral initialization and pin configuration
 *
 * SAFETY FEATURES:
 * - Checkpoint debugging at every step
 * - WDT feeding to prevent resets
 * - Validation after each operation
 * - Pre/post register state dumps
 */

#include <stdio.h>
#include "esp_log.h"
#include "esp_console.h"
#include "driver/sdmmc_host.h"
#include "driver/gpio.h"
#include "soc/io_mux_reg.h"
#include "soc/gpio_sig_map.h"
#include "esp_rom_gpio.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "sd_init";

// Helper to read and display current IO_MUX state
static void dump_pin_state(int gpio_num, const char *name) {
    uint32_t iomux_reg;

    switch(gpio_num) {
        case 2:  iomux_reg = REG_READ(PERIPHS_IO_MUX_GPIO2_U); break;
        case 14: iomux_reg = REG_READ(PERIPHS_IO_MUX_MTMS_U); break;
        case 15: iomux_reg = REG_READ(PERIPHS_IO_MUX_MTDO_U); break;
        default: return;
    }

    int func_sel = (iomux_reg >> 12) & 0x7;
    int ie = (iomux_reg >> 8) & 0x1;
    int oe = (iomux_reg >> 10) & 0x1;
    bool pullup = (iomux_reg >> 7) & 0x1;
    bool pulldown = (iomux_reg >> 6) & 0x1;

    printf("    GPIO%-2d (%-4s): func=%d ie=%d oe=%d pu=%d pd=%d [0x%08lX]\n",
           gpio_num, name, func_sel, ie, oe, pullup, pulldown, iomux_reg);
}

// Check if SDMMC peripheral is alive
static bool check_sdmmc_alive(void) {
    volatile uint32_t *sdmmc_base = (volatile uint32_t *)0x3FF68000;
    uint32_t ctrl = sdmmc_base[0x000 / 4];
    uint32_t status = sdmmc_base[0x048 / 4];

    printf("    SDMMC CTRL=0x%08lX STATUS=0x%08lX\n", ctrl, status);

    // If both are zero, peripheral is dead
    return (ctrl != 0 || status != 0);
}

// Force IO_MUX to SDMMC function using standard ESP32 macros
// NOTE: On ESP32, SDMMC uses func=4 for GPIO2/14/15 (not func=1!)
static void force_iomux_sdmmc(void) {
    printf("\n=== Forcing IO_MUX to SDMMC (func=4) Using PIN_FUNC_SELECT ===\n");

    // GPIO2 = SD_DATA0 (func=4)
    printf("  GPIO2  (D0):  ");
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2_SD_DATA0);  // func=4
    gpio_set_pull_mode(GPIO_NUM_2, GPIO_PULLUP_ONLY);
    printf("Set to FUNC_GPIO2_SD_DATA0 (func=4) with pull-up\n");

    // GPIO14 = SD_CLK (func=4)
    printf("  GPIO14 (CLK): ");
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTMS_U, FUNC_MTMS_SD_CLK);  // func=4
    printf("Set to FUNC_MTMS_SD_CLK (func=4)\n");

    // GPIO15 = SD_CMD (func=4)
    printf("  GPIO15 (CMD): ");
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDO_U, FUNC_MTDO_SD_CMD);  // func=4
    gpio_set_pull_mode(GPIO_NUM_15, GPIO_PULLUP_ONLY);
    printf("Set to FUNC_MTDO_SD_CMD (func=4) with pull-up\n");

    // GPIO4 = SD_DATA1 (func=4) - for 4-bit mode
    printf("  GPIO4  (D1):  ");
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO4_U, FUNC_GPIO4_SD_DATA1);  // func=4
    gpio_set_pull_mode(GPIO_NUM_4, GPIO_PULLUP_ONLY);
    printf("Set to FUNC_GPIO4_SD_DATA1 (func=4) with pull-up\n");

    // GPIO12 = SD_DATA2 (func=4) - for 4-bit mode
    printf("  GPIO12 (D2):  ");
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTDI_U, FUNC_MTDI_SD_DATA2);  // func=4
    gpio_set_pull_mode(GPIO_NUM_12, GPIO_PULLUP_ONLY);
    printf("Set to FUNC_MTDI_SD_DATA2 (func=4) with pull-up\n");

    // GPIO13 = SD_DATA3 (func=4) - for 4-bit mode
    printf("  GPIO13 (D3):  ");
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_MTCK_SD_DATA3);  // func=4
    gpio_set_pull_mode(GPIO_NUM_13, GPIO_PULLUP_ONLY);
    printf("Set to FUNC_MTCK_SD_DATA3 (func=4) with pull-up\n");

    printf("  ✅ IO_MUX configured for 4-bit SDMMC mode\n");

    // Validate
    printf("\n  Validating configuration:\n");
    dump_pin_state(2, "D0");
    dump_pin_state(4, "D1");
    dump_pin_state(12, "D2");
    dump_pin_state(13, "D3");
    dump_pin_state(14, "CLK");
    dump_pin_state(15, "CMD");
}

static int do_sd_init(int argc, char **argv)
{
    printf("\n╔════════════════════════════════════════════╗\n");
    printf("║   SDMMC Explicit Initialization (Safe)     ║\n");
    printf("╚════════════════════════════════════════════╝\n\n");

    printf("This command will:\n");
    printf("  1. Show baseline state (func=? regs=?)\n");
    printf("  2. Initialize SDMMC host peripheral\n");
    printf("  3. Initialize SDMMC slot 0 (1-bit mode)\n");
    printf("  4. Force IO_MUX to func=1 for SDMMC pins\n");
    printf("  5. Validate final state\n\n");

    printf("SAFETY FEATURES:\n");
    printf("  - WDT feeding at each step\n");
    printf("  - Checkpoint debugging\n");
    printf("  - State validation\n\n");

    // === CHECKPOINT 0: Baseline State ===
    printf("╔═══ CHECKPOINT 0: Baseline State ═══\n");
    printf("  Current pin configuration:\n");
    dump_pin_state(2, "D0");
    dump_pin_state(14, "CLK");
    dump_pin_state(15, "CMD");
    printf("  SDMMC peripheral state:\n");
    bool alive_before = check_sdmmc_alive();
    printf("  Peripheral: %s\n", alive_before ? "ALIVE" : "DEAD (registers all zero)");
    esp_task_wdt_reset();  // Feed WDT

    // === CHECKPOINT 1: Reset Pins ===
    printf("\n╔═══ CHECKPOINT 1: Reset Pins ═══\n");
    printf("  Calling gpio_reset_pin()...\n");
    gpio_reset_pin(GPIO_NUM_2);
    printf("    ✓ GPIO2 reset\n");
    esp_task_wdt_reset();

    gpio_reset_pin(GPIO_NUM_14);
    printf("    ✓ GPIO14 reset\n");
    esp_task_wdt_reset();

    gpio_reset_pin(GPIO_NUM_15);
    printf("    ✓ GPIO15 reset\n");
    esp_task_wdt_reset();

    printf("  ✓ All pins reset to GPIO mode\n");

    // === CHECKPOINT 2: Initialize SDMMC Host ===
    printf("\n╔═══ CHECKPOINT 2: Initialize SDMMC Host ═══\n");
    printf("  Calling sdmmc_host_init()...\n");
    printf("  (If lockup occurs here, host init is blocking)\n");
    fflush(stdout);  // Force output before potential lockup

    esp_err_t ret = sdmmc_host_init();
    esp_task_wdt_reset();

    if (ret != ESP_OK) {
        printf("  ✗ sdmmc_host_init() FAILED: %s (0x%x)\n", esp_err_to_name(ret), ret);
        printf("  ERROR: SDMMC peripheral couldn't be initialized!\n");
        printf("  Check: sdkconfig CONFIG_ESP32_SDMMC_* settings\n");
        return 1;
    }
    printf("  ✓ sdmmc_host_init() succeeded (no lockup!)\n");

    // Check if peripheral is now alive
    printf("  Checking if peripheral woke up:\n");
    bool alive_after_host = check_sdmmc_alive();
    printf("  Peripheral: %s\n", alive_after_host ? "ALIVE!" : "still dead");

    // === CHECKPOINT 3: Initialize SDMMC Slot ===
    printf("\n╔═══ CHECKPOINT 3: Initialize SDMMC Slot 0 ═══\n");
    printf("  Configuring slot (1-bit, no CD/WP)...\n");
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1;  // 1-bit mode (only D0)
    slot_config.cd = SDMMC_SLOT_NO_CD;  // No card detect
    slot_config.wp = SDMMC_SLOT_NO_WP;  // No write protect
    slot_config.flags = SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

    // CRITICAL: Print slot_config structure to detect bad configurations
    printf("\n  Slot config structure contents:\n");
    printf("    width=%d (1=1-bit, 4=4-bit)\n", slot_config.width);
    printf("    cd=%d (card detect: %d=disabled)\n", slot_config.cd, SDMMC_SLOT_NO_CD);
    printf("    wp=%d (write protect: %d=disabled)\n", slot_config.wp, SDMMC_SLOT_NO_WP);
    printf("    flags=0x%lX (0x%lX=internal pullups)\n", (unsigned long)slot_config.flags, (unsigned long)SDMMC_SLOT_FLAG_INTERNAL_PULLUP);

    // Validate configuration BEFORE calling init
    printf("\n  Validating slot configuration...\n");
    bool config_error = false;

    if (slot_config.width != 1) {
        printf("    ⚠ Width mismatch! Expected 1, got %d\n", slot_config.width);
        config_error = true;
    }
    if (slot_config.cd != SDMMC_SLOT_NO_CD) {
        printf("    ⚠ Card detect not disabled! cd=%d (expected %d)\n", slot_config.cd, SDMMC_SLOT_NO_CD);
        config_error = true;
    }
    if (slot_config.wp != SDMMC_SLOT_NO_WP) {
        printf("    ⚠ Write protect not disabled! wp=%d (expected %d)\n", slot_config.wp, SDMMC_SLOT_NO_WP);
        config_error = true;
    }

    if (config_error) {
        printf("  ✗ Configuration errors detected! May cause lockup.\n");
        printf("  Aborting to prevent hardware exception.\n");
        sdmmc_host_deinit();
        return 1;
    } else {
        printf("  ✓ Slot configuration valid\n");
        printf("  NOTE: ESP32 uses fixed pins for SDMMC Slot 0:\n");
        printf("    CLK=GPIO14, CMD=GPIO15, D0=GPIO2 (hardwired in silicon)\n");
    }

    printf("\n  Calling sdmmc_host_init_slot()...\n");
    printf("  (If lockup occurs here, slot init has internal issue)\n");
    printf("  Using SLOT_1 for eFuse-remapped pins (SLOT_0 may not work with custom pins)\n");
    fflush(stdout);

    ret = sdmmc_host_init_slot(SDMMC_HOST_SLOT_1, &slot_config);
    esp_task_wdt_reset();

    if (ret != ESP_OK) {
        printf("  ✗ sdmmc_host_init_slot() FAILED: %s (0x%x)\n", esp_err_to_name(ret), ret);
        printf("  ERROR: Slot configuration failed!\n");
        printf("  Cleaning up...\n");
        sdmmc_host_deinit();
        return 1;
    }
    printf("  ✓ sdmmc_host_init_slot() succeeded (no lockup!)\n");

    // Check pin state after slot init
    printf("  Pin state after slot init:\n");
    dump_pin_state(2, "D0");
    dump_pin_state(14, "CLK");
    dump_pin_state(15, "CMD");

    // === CHECKPOINT 4: Force IO_MUX (if needed) ===
    printf("\n╔═══ CHECKPOINT 4: Force IO_MUX (Validation) ═══\n");

    // Check if ESP-IDF already set func=4 (SDMMC)
    uint32_t reg2 = REG_READ(PERIPHS_IO_MUX_GPIO2_U);
    int func2 = (reg2 >> 12) & 0x7;

    if (func2 == 4) {
        printf("  ✓ ESP-IDF already set func=4 (SDMMC)! No manual override needed.\n");
        printf("  Validating:\n");
        dump_pin_state(2, "D0");
        dump_pin_state(4, "D1");
        dump_pin_state(12, "D2");
        dump_pin_state(13, "D3");
        dump_pin_state(14, "CLK");
        dump_pin_state(15, "CMD");
    } else {
        printf("  ⚠ func=%d (expected 4 for SDMMC), forcing override...\n", func2);
        force_iomux_sdmmc();
    }

    esp_task_wdt_reset();

    // === CHECKPOINT 5: Final Validation ===
    printf("\n╔═══ CHECKPOINT 5: Final Validation ═══\n");
    printf("  Final SDMMC peripheral state:\n");
    bool alive_final = check_sdmmc_alive();
    printf("  Peripheral: %s\n", alive_final ? "✅ ALIVE!" : "✗ DEAD");

    printf("  Final pin configuration:\n");
    dump_pin_state(2, "D0");
    dump_pin_state(4, "D1");
    dump_pin_state(12, "D2");
    dump_pin_state(13, "D3");
    dump_pin_state(14, "CLK");
    dump_pin_state(15, "CMD");

    // Overall result
    printf("\n╔════════════════════════════════════════════╗\n");
    if (alive_final) {
        uint32_t reg2_final = REG_READ(PERIPHS_IO_MUX_GPIO2_U);
        int func2_final = (reg2_final >> 12) & 0x7;

        if (func2_final == 4) {
            printf("║  ✅ SUCCESS! SDMMC PERIPHERAL READY!        ║\n");
        } else {
            printf("║  ⚠ PARTIAL: Peripheral alive, func=%d      ║\n", func2_final);
        }
    } else {
        printf("║  ✗ FAILED: Peripheral still dead            ║\n");
    }
    printf("╚════════════════════════════════════════════╝\n\n");

    printf("Next steps:\n");
    printf("  1. Run: sdmmc_regs (full register dump)\n");
    printf("  2. If func=4 and regs non-zero:\n");
    printf("     → Try: sd_nocd (attempt mount)\n");
    printf("  3. If mount times out:\n");
    printf("     → Hardware path issue (FPGA/level shifter)\n\n");

    return 0;
}

void register_sd_init(void)
{
    const esp_console_cmd_t cmd = {
        .command = "sd_init",
        .help = "Explicitly initialize SDMMC peripheral and force pin configuration",
        .hint = NULL,
        .func = &do_sd_init,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}
