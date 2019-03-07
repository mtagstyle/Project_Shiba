#include "image_processor.h"

led_rgb_t ConvertDepthTo24(uint8_t rgb)
{
    led_rbg_t ret_val = {0};

    // Get individual colors
    ret_val.r = ((rgb & 0b11100000) >> 5); // Mask out red, downshift
    ret_val.g = (rgb & 0b00011100) >> 2;   // Mask out green, downshift
    ret_val.b = (rgb & 0b00000011);        // Mask out blue

    // Rescale to 24 bit-depth
    ret_val.r = ret_val.r * (32);     // Max value of 8 bits/Max value of 3 bits
    ret_val.g = ret_val.g * (32);     // Max value of 8 bits/Max value of 3 bits
    ret_val.b = ret_val.b * (64);     // Max value of 8 bits/Max value of 2 bits

    return ret_val;
}

led_rgb_t GetLedInfo(const image_t *img, uint8_t pattern_number, uint8_t led_number, led_quadrant_map_t quadrant)
{
    led_rgb_t ret_val = {0};
    int row = -1;
    int col = -1;

    if((led_number < LEDS_PER_STRIP) && (pattern_number < BITMATRIX_PATTERNS))
    {
        // Determine which quadrant, and get coordinates WRT img
        switch(quadrant)
        {
            case QUAD_TOP_LEFT:
                col = pattern_number;
                row = (IMG_ROWS_DIV2-1) - led_number;
            break;
            case QUAD_BOTTOM_LEFT:
                col = pattern_number;
                row = (IMG_ROWS_DIV2) + led_number;
            break;
            case QUAD_TOP_RIGHT:
                col = (IMG_COLS_DIV2 + pattern_number) % BITMATRIX_PATTERNS;
                row = (IMG_ROWS_DIV2-1) - led_number;
            break;
            case QUAD_BOTTOM_RIGHT:
                col = (IMG_COLS_DIV2 + pattern_number) % BITMATRIX_PATTERNS;
                row = (IMG_ROWS_DIV2) + led_number;
            break;
        }

        if(row != -1 && col != -1)
        {
            ret_val = ConvertDepthTo24(img[row][col]);
        }
    }

    return ret_val;
}

bool MapImageToBitPattern(gpio_bitstream_t *dst, const image_t *img, uint8_t pattern_number)
{
    bool result = true;
    int bitstream_idx = -1;

    if(dst != NULL && img != NULL)
    {
        // Don't use sizeof array here because it might just use the sizeof a pointer
        memset(dst, 0, sizeof(uint8_t) * BITSTREAM_LEN);

        // Iterate through each byte in the bitstream, and begin to map the image against the bitstream
        for(uint8_t i = 0; i < LEDS_PER_STRIP; i++)
        {
            // Get the 24-bit R,G,B value for every LED at a specific pattern, and LED number
            led_rgb_t leds[NUM_STRIPS] = {0};
            for(uint8_t strip = 0; strip < NUM_STRIPS; strip++)
            {
                leds[strip] = GetLedInfo(img, pattern_number, i, strip);
            }

            // Begin to write the bits of each LED to the bitstream
            for(uint8_t j = 0; j < sizeof(led_rgb_t); j++)
            {
                // Reading red, green or blue (The order matters because the datasheet says write G,R,B)
                uint8_t bytes[NUM_STRIPS] = {0};
                
                for(uint8_t strip = 0; strip < NUM_STRIPS; strip++)
                {
                    switch(j)
                    {
                        case 0:
                            bytes[strip] = leds[strip].g;
                        break;
                        case 1:
                            bytes[strip] = leds[strip].r;
                        break;
                        case 2:
                            bytes[strip] = leds[strip].b;
                        break;
                    }
                }

                // Begin writing from MSB to LSB, for each strip
                for(uint8_t k = 0; k < 8; k++)
                {
                    uint8_t masked_bit = (bytes[strip] << k) & 0x80;

                    // Every even bit on the LED strip is on the upper half of the byte,
                    // and every odd bit on the LED strip is on the bottom half of the byte

                    // The the i_th LED is in the i_th bit position
                    if(k%2 == 0)
                    {
                        masked_bit = masked_bit >> strip;
                        bitstream_idx++;
                    }
                    else
                    {
                        masked_bit = masked_bit >> 4 + strip;
                    }
                    
                    // Write it
                    dst[bitstream_idx] |= masked_bit;
                }
            }
        }
    }
    else
    {
        result = false;
    }
    
    return result;
}

bool SaveImageToFlash(const image_quadrant_t *img)
{
    bool result = true;

    // Iterate through all possible bit patterns
    for(uint32_t i = 0; i < BITMATRIX_PATTERNS; i++)
    {
        // Populate the bitmatrix
        gpio_bitstream_t bitstream;
        if(MapImageToBitPattern(bitstream, img, i) == false)
        {
            result = false;
            break;
        }

        // Save that pattern to flash
    }

    return result;
}

/*
void image_data_to_driver_data(combined_bitstream_t* driver_data, led_rgb_t* image_data[NUM_STRIPS][LEDS_PER_STRIP])
{
	if(image_data != NULL && driver_data != NULL)
	{
		// For each byte in the LED strip (Each LED is 3 bytes long (RGB))
		for(uint8_t i = 0; i < LEDS_PER_STRIP*sizeof(led_rgb_t); i++)
		{
			uint8_t current_unmasked_bits0 = (*((uint8_t*)&(image_data[0][0]) + (i)));
			uint8_t current_unmasked_bits1 = (*((uint8_t*)&(image_data[1][0]) + (i)));
			uint8_t current_unmasked_bits2 = (*((uint8_t*)&(image_data[2][0]) + (i)));
			uint8_t current_unmasked_bits3 = (*((uint8_t*)&(image_data[3][0]) + (i)));

			// For each bit in the byte
			uint8_t num_shift = 7;
			for(uint8_t j = 0x80; j >= 0x01; j = j >> 1)
			{
				// Storage for masking the bits
				uint8_t current_masked_bits0 = (current_unmasked_bits0 & j) >> (num_shift);		// Bit position 0
				uint8_t current_masked_bits1 = (current_unmasked_bits1 & j) >> (num_shift - 1); // Bit position 1
				uint8_t current_masked_bits2 = (current_unmasked_bits2 & j) >> (num_shift - 2); // Bit position 2
				uint8_t current_masked_bits3 = (current_unmasked_bits3 & j) >> (num_shift - 3); // Bit position 3

				uint8_t result = current_masked_bits0 | current_masked_bits1 | current_masked_bits2 | current_masked_bits3;
				uint8_t gpio_array = 0; 



				driver_data->bitstream[0][(i*8)+(7-num_shift)] = gpio_array;

				num_shift--;
			}
		}
	}
}
*/
