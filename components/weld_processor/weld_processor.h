#pragma once

#include "esp_err.h"

typedef enum {
    WELD_STATE_WAITING    = 0,  /* USB MSC ready; no host writes seen since last cycle */
    WELD_STATE_WRITING    = 1,  /* Host is actively writing blocks */
    WELD_STATE_PROCESSING = 2,  /* ESP holds SD ownership; reading files */
    WELD_STATE_SUCCESS    = 3,  /* Processing complete; placeholder written */
    WELD_STATE_FAILURE    = 4,  /* Mount or file I/O error; waiting for next write cycle */
} weld_state_t;

/*
 * Start the write-idle monitor task.  Must be called after usb_msc_sd_init().
 * LCD is set to CYAN (waiting) immediately.  The monitor task triggers SD
 * processing after WELD_IDLE_WINDOW_MS of no USB MSC writes.
 *
 * Known limitation: detects block-write idle at the USB SCSI level, not a
 * true Kawasaki file-close event.  5000 ms idle window used for Stage 5.
 */
esp_err_t weld_processor_start(void);
