#include "usb_msc_sd.h"

#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/sdspi_host.h"
#include "sdmmc_cmd.h"
#include "tinyusb.h"
#include "tusb_msc_storage.h"

static const char *TAG = "usb_msc_sd";

/* SD uses SPI3_HOST; LCD occupies SPI2_HOST — they must not share a bus. */
#define SD_SPI_HOST    SPI3_HOST
#define SD_MOUNT_POINT "/sdcard"

static sdmmc_card_t *s_card = NULL;

static esp_err_t sd_spi_init(const usb_msc_sd_config_t *cfg)
{
    spi_bus_config_t bus_cfg = {
        .mosi_io_num     = cfg->mosi_pin,
        .miso_io_num     = cfg->miso_pin,
        .sclk_io_num     = cfg->clk_pin,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = 4096,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    ESP_ERROR_CHECK(sdspi_host_init());

    sdspi_device_config_t dev_cfg = SDSPI_DEVICE_CONFIG_DEFAULT();
    dev_cfg.host_id = SD_SPI_HOST;
    dev_cfg.gpio_cs = (gpio_num_t)cfg->cs_pin;

    sdspi_dev_handle_t dev_handle;
    ESP_ERROR_CHECK(sdspi_host_init_device(&dev_cfg, &dev_handle));

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = dev_handle;

    s_card = malloc(sizeof(sdmmc_card_t));
    if (!s_card) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t ret = sdmmc_card_init(&host, s_card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SD card probe failed: %s", esp_err_to_name(ret));
        free(s_card);
        s_card = NULL;
        return ret;
    }
    sdmmc_card_print_info(stdout, s_card);
    return ESP_OK;
}

esp_err_t usb_msc_sd_init(const usb_msc_sd_config_t *cfg)
{
    ESP_LOGI(TAG, "SD SPI init — CLK=%d MOSI=%d MISO=%d CS=%d",
             cfg->clk_pin, cfg->mosi_pin, cfg->miso_pin, cfg->cs_pin);

    esp_err_t ret = sd_spi_init(cfg);
    if (ret != ESP_OK) {
        return ret;
    }

    /* Register SD card as TinyUSB MSC storage and mount FAT VFS. */
    const tinyusb_msc_sdmmc_config_t msc_cfg = {
        .card = s_card,
        .mount_config.max_files = 5,
    };
    ESP_ERROR_CHECK(tinyusb_msc_storage_init_sdmmc(&msc_cfg));
    ESP_ERROR_CHECK(tinyusb_msc_storage_mount(SD_MOUNT_POINT));

    /* Install TinyUSB driver.  Null descriptors → default composite CDC+MSC
     * descriptor built from Kconfig (CONFIG_TINYUSB_CDC_ENABLED +
     * CONFIG_TINYUSB_MSC_ENABLED).  This call switches the USB PHY from
     * Serial/JTAG to OTG; USB Serial JTAG console stops after this point. */
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor        = NULL,
        .string_descriptor        = NULL,
        .string_descriptor_count  = 0,
        .external_phy             = false,
        .configuration_descriptor = NULL,
    };
    ESP_LOGI(TAG, "Calling tinyusb_driver_install...");
    esp_err_t tusb_ret = tinyusb_driver_install(&tusb_cfg);
    ESP_LOGI(TAG, "tinyusb_driver_install returned: %s (0x%x)", esp_err_to_name(tusb_ret), (unsigned)tusb_ret);
    if (tusb_ret != ESP_OK) {
        ESP_LOGE(TAG, "TinyUSB driver install FAILED — USB PHY will not switch to OTG");
        return tusb_ret;
    }

    ESP_LOGI(TAG, "USB MSC+CDC ready — SD mounted at %s", SD_MOUNT_POINT);
    return ESP_OK;
}
