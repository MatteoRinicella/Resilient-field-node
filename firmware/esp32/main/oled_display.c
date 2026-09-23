#include "oled_display.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define OLED_WIDTH 128
#define OLED_PAGE_COUNT 8
#define OLED_COLUMN_OFFSET 2
#define OLED_TIMEOUT_MS 1000

#define OLED_CONTROL_COMMAND 0x00
#define OLED_CONTROL_DATA 0x40

static uint8_t oled_framebuffer[OLED_WIDTH * OLED_PAGE_COUNT];

static const uint8_t font_digits[10][5] = {
  {0x3E, 0x51, 0x49, 0x45, 0x3E},
  {0x00, 0x42, 0x7F, 0x40, 0x00},
  {0x42, 0x61, 0x51, 0x49, 0x46},
  {0x21, 0x41, 0x45, 0x4B, 0x31},
  {0x18, 0x14, 0x12, 0x7F, 0x10},
  {0x27, 0x45, 0x45, 0x45, 0x39},
  {0x3C, 0x4A, 0x49, 0x49, 0x30},
  {0x01, 0x71, 0x09, 0x05, 0x03},
  {0x36, 0x49, 0x49, 0x49, 0x36},
  {0x06, 0x49, 0x49, 0x29, 0x1E}
};


static const uint8_t font_uppercase[26][5] = {
  {0x7E, 0x11, 0x11, 0x11, 0x7E},
  {0x7F, 0x49, 0x49, 0x49, 0x36},
  {0x3E, 0x41, 0x41, 0x41, 0x22},
  {0x7F, 0x41, 0x41, 0x22, 0x1C},
  {0x7F, 0x49, 0x49, 0x49, 0x41},
  {0x7F, 0x09, 0x09, 0x09, 0x01},
  {0x3E, 0x41, 0x49, 0x49, 0x7A},
  {0x7F, 0x08, 0x08, 0x08, 0x7F},
  {0x00, 0x41, 0x7F, 0x41, 0x00},
  {0x20, 0x40, 0x41, 0x3F, 0x01},
  {0x7F, 0x08, 0x14, 0x22, 0x41},
  {0x7F, 0x40, 0x40, 0x40, 0x40},
  {0x7F, 0x02, 0x0C, 0x02, 0x7F},
  {0x7F, 0x04, 0x08, 0x10, 0x7F},
  {0x3E, 0x41, 0x41, 0x41, 0x3E},
  {0x7F, 0x09, 0x09, 0x09, 0x06},
  {0x3E, 0x41, 0x51, 0x21, 0x5E},
  {0x7F, 0x09, 0x19, 0x29, 0x46},
  {0x46, 0x49, 0x49, 0x49, 0x31},
  {0x01, 0x01, 0x7F, 0x01, 0x01},
  {0x3F, 0x40, 0x40, 0x40, 0x3F},
  {0x1F, 0x20, 0x40, 0x20, 0x1F},
  {0x7F, 0x20, 0x18, 0x20, 0x7F},
  {0x63, 0x14, 0x08, 0x14, 0x63},
  {0x03, 0x04, 0x78, 0x04, 0x03},
  {0x61, 0x51, 0x49, 0x45, 0x43}
};


static const uint8_t glyph_space[5] = {
  0x00, 0x00, 0x00, 0x00, 0x00
};


static const uint8_t glyph_colon[5] = {
  0x00, 0x36, 0x36, 0x00, 0x00
};


static const uint8_t glyph_dot[5] = {
  0x00, 0x60, 0x60, 0x00, 0x00
};


static const uint8_t glyph_minus[5] = {
  0x08, 0x08, 0x08, 0x08, 0x08
};


static const uint8_t glyph_unknown[5] = {
  0x02, 0x01, 0x51, 0x09, 0x06
};

static const uint8_t glyph_lowercase_m[5] = {
  0x7C, 0x04, 0x18, 0x04, 0x78
};

static const uint8_t *oled_get_glyph(char character)
{
    if(character >= '0' && character <= '9')
    {
        return font_digits[character - '0'];
    }

    if(character >= 'A' && character <= 'Z')
    {
        return font_uppercase[character - 'A'];
    }

    if(character == 'm')
    {
        return glyph_lowercase_m;
    }

    if(character == ' ')
    {
        return glyph_space;
    }

    if(character == ':')
    {
        return glyph_colon;
    }

    if(character == '.')
    {
        return glyph_dot;
    }

    if(character == '-')
    {
        return glyph_minus;
    }

    return glyph_unknown;
}

static esp_err_t oled_send_commands(
    i2c_master_dev_handle_t oled_device,
    const uint8_t *commands,
    size_t command_count
)
{
    uint8_t packet[32];

    if(command_count +1 > sizeof(packet))
    {
        return ESP_ERR_INVALID_SIZE;
    }

    packet[0] = OLED_CONTROL_COMMAND;

    memcpy(
        &packet[1],
        commands,
        command_count
    );
    
    return i2c_master_transmit(
        oled_device,
        packet,
        command_count +1,
        OLED_TIMEOUT_MS
    );
}

    static void oled_draw_character(
        uint8_t x,
        uint8_t page,
        char character
    )
    {
        if(page >= OLED_PAGE_COUNT || x + 5 >= OLED_WIDTH)
        {
            return;
        }

        const uint8_t *glyph= oled_get_glyph(character);

        for( size_t column = 0; column < 5; column ++)
        {
            oled_framebuffer[page * OLED_WIDTH + x + column] = glyph[column];
        }

        oled_framebuffer[page * OLED_WIDTH + x + 5] = 0x00;
    }

    static void oled_draw_text(
        uint8_t x,
        uint8_t page,
        const char *text
    )
    {
        while(*text != '\0' && x + 5 < OLED_WIDTH)
        {
            oled_draw_character(
                x,
                page,
                *text
            );

            x += 6;
            text ++;
        }
    }

    static esp_err_t oled_flush(
  i2c_master_dev_handle_t oled_device
)
{
  uint8_t data_packet[OLED_WIDTH + 1];

  data_packet[0] = OLED_CONTROL_DATA;

  for (
    uint8_t page = 0;
    page < OLED_PAGE_COUNT;
    page++
  )
  {
    const uint8_t page_commands[] = {
      (uint8_t)(0xB0 | page),
      (uint8_t)(
        OLED_COLUMN_OFFSET & 0x0F
      ),
      (uint8_t)(
        0x10 |
        (OLED_COLUMN_OFFSET >> 4)
      )
    };

    const esp_err_t command_result =
      oled_send_commands(
        oled_device,
        page_commands,
        sizeof(page_commands)
      );

    if (command_result != ESP_OK)
    {
      return command_result;
    }

    memcpy(
      &data_packet[1],
      &oled_framebuffer[
        page * OLED_WIDTH
      ],
      OLED_WIDTH
    );

    const esp_err_t data_result =
      i2c_master_transmit(
        oled_device,
        data_packet,
        sizeof(data_packet),
        OLED_TIMEOUT_MS
      );

    if (data_result != ESP_OK)
    {
      return data_result;
    }
  }

  return ESP_OK;
}


esp_err_t oled_display_initialize(
    i2c_master_dev_handle_t oled_device
)
{
    const uint8_t initialization_commands[] = 
    {
        0xAE,
        0xD5, 0x80,
        0xA8, 0x3F,
        0xD3, 0x00,
        0x40,
        0xAD, 0x8B,
        0xA1,
        0xC8,
        0xDA, 0x12,
        0x81, 0x80,
        0xD9, 0x22,
        0xDB, 0x35,
        0xA4,
        0xA6,
        0xAF
    };

    const esp_err_t initialization_result =
    oled_send_commands(
        oled_device,
        initialization_commands,
        sizeof(initialization_commands)
    );
    
    if(initialization_result != ESP_OK)
    {
        return initialization_result;
    }

    memset(
        oled_framebuffer,
        0,
        sizeof(oled_framebuffer)
    );

    return oled_flush(oled_device);
}

esp_err_t oled_display_show_status(
    i2c_master_dev_handle_t oled_device,
    double current_ma,
    double threshold_ma,
    bool tripped,
    BreakerState breaker_state
)
{
    char text_line[32];

    memset(
        oled_framebuffer,
        0,
        sizeof(oled_framebuffer)
    );

    snprintf(
        text_line,
        sizeof(text_line),
        "CORRENTE: %.1f mA",
        current_ma
    );

    oled_draw_text(
        0,
        0,
        text_line
    );

    snprintf(
       text_line,
        sizeof(text_line),
        "SOGLIA: %.1f mA",
        threshold_ma 
    );

    oled_draw_text(
        0,
        2,
        text_line
    );
    
    oled_draw_text(
        0,
        4,
        tripped
        ? "PROTEZIONE: SCATTATA"
        : "PROTEZIONE: NORMALE"
    );

    oled_draw_text(
        0,
        6,
        breaker_state == BREAKER_STATE_CLOSED
        ? "CARICO: ALIMENTATO"
        : "CARICO: SCOLLEGATO"
    );

    return oled_flush(oled_device);
}