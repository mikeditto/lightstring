// This program is an "Arduino+NeoPixel Emulator".
//
// It allows developing and testing simple Arduino sketches that produce
// NeoPixel LED string displays and effects, running them on an ordinary
// PC and showing them on the computer screen using SDL (Simple
// DirectMedia Layer).

#include <SDL.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// This class is a stripped-down copy of Adafruit_NeoPixel for Arduino
// modified to use SDL2 as the rendering backend.  License: GPLv3
// https://github.com/adafruit/Adafruit_NeoPixel
// Some inspiration was taken from SDL tutorials by Lazy Foo' Productions.
// http://lazyfoo.net/tutorials/SDL/index.php#Hello%20SDL
class Fake_NeoPixel {
public:
    Fake_NeoPixel(uint16_t n, uint16_t width, uint16_t height)
	: width(width), height(height), pixels(NULL), window(NULL) {
	updateLength(n);
    }

    ~Fake_NeoPixel() {
	free(pixels);

	if (window) {
	    SDL_DestroyWindow(window);
	    SDL_Quit();
	}
    }

    void begin(void) {
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
	    fprintf(stderr, "SDL could not initialize! SDL_Error: %s\n",
		    SDL_GetError());
	    exit(1);
	}

	window = SDL_CreateWindow("Fake_NeoPixel String",
				  SDL_WINDOWPOS_UNDEFINED,
				  SDL_WINDOWPOS_UNDEFINED, width, height,
				  SDL_WINDOW_SHOWN);
	if (window == NULL) {
	    fprintf(stderr, "Window could not be created! SDL_Error: %s\n",
		    SDL_GetError());
	    exit(1);
	}

	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    };

    void updateLength(uint16_t n) {
	free(pixels); // Free existing data (if any)

	// Allocate new data -- note: ALL PIXELS ARE CLEARED
	numLEDs = n;
	pixels = (uint8_t *)calloc(numLEDs, 3);
	if (!pixels)
	    numLEDs = 0;
    }

    void show(void) {
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0xFF);
	SDL_RenderClear(renderer);

	uint16_t step = width / numLEDs;
	uint16_t size = step / 2;
	uint16_t x = size / 2;
	uint16_t y = (height - size) / 2;

	const uint8_t *p = pixels;
	for (int i=0; i<numLEDs; ++i) {
	    SDL_SetRenderDrawColor(renderer, p[0], p[1], p[2], 0xFF);
	    p += 3;
	    SDL_Rect fillRect = { x, y, size, size };
	    SDL_RenderFillRect(renderer, &fillRect);
	    x += step;
	}

	SDL_RenderPresent(renderer);
	SDL_UpdateWindowSurface(window);
    }

    /*!
      @brief   Set a pixel's color using a 32-bit 'packed' RGB or RGBW value.
      @param   n  Pixel index, starting from 0.
      @param   c  32-bit color value. Most significant byte is white (for RGBW
		  pixels) or ignored (for RGB pixels), next is red, then green,
		  and least significant byte is blue.
    */
    void setPixelColor(uint16_t n, uint32_t c) {
	if (n < numLEDs) {
	    uint8_t *p;
	    uint8_t r = (uint8_t)(c >> 16),
		    g = (uint8_t)(c >> 8),
		    b = (uint8_t)c;
	    if (brightness) { // See notes in setBrightness()
		r = (r * brightness) >> 8;
		g = (g * brightness) >> 8;
		b = (b * brightness) >> 8;
	    }
	    p = &pixels[n * 3];
	    p[0] = r;
	    p[1] = g;
	    p[2] = b;
	}
    }

    /*!
      @brief   Fill all or part of the NeoPixel strip with a color.
      @param   c      32-bit color value. Most significant byte is white (for
		      RGBW pixels) or ignored (for RGB pixels), next is red,
		      then green, and least significant byte is blue. If all
		      arguments are unspecified, this will be 0 (off).
      @param   first  Index of first pixel to fill, starting from 0. Must be
		      in-bounds, no clipping is performed. 0 if unspecified.
      @param   count  Number of pixels to fill, as a positive value. Passing
		      0 or leaving unspecified will fill to end of strip.
    */
    void fill(uint32_t c, uint16_t first, uint16_t count) {
	uint16_t i, end;

	if (first >= numLEDs) {
	    return; // If first LED is past end of strip, nothing to do
	}

	// Calculate the index ONE AFTER the last pixel to fill
	if (count == 0) {
	    // Fill to end of strip
	    end = numLEDs;
	} else {
	    // Ensure that the loop won't go past the last pixel
	    end = first + count;
	    if (end > numLEDs)
		end = numLEDs;
	}

	for (i = first; i < end; i++)
	    this->setPixelColor(i, c);
    }

    void clear(void) { memset(pixels, 0, numLEDs * 3); }

    /*!
      @brief   Retrieve the last-set brightness value for the strip.
      @return  Brightness value: 0 = minimum (off), 255 = maximum.
    */
    uint8_t getBrightness(void) const { return brightness - 1; }

    /*!
      @brief   Adjust output brightness. Does not immediately affect what's
	       currently displayed on the LEDs. The next call to show() will
	       refresh the LEDs at this level.
      @param   b  Brightness setting, 0=minimum (off), 255=brightest.
      @note    This was intended for one-time use in one's setup() function,
	       not as an animation effect in itself. Because of the way this
	       library "pre-multiplies" LED colors in RAM, changing the
	       brightness is often a "lossy" operation -- what you write to
	       pixels isn't necessary the same as what you'll read back.
	       Repeated brightness changes using this function exacerbate the
	       problem. Smart programs therefore treat the strip as a
	       write-only resource, maintaining their own state to render each
	       frame of an animation, not relying on read-modify-write.
    */
    void setBrightness(uint8_t b) {
	// Stored brightness value is different than what's passed.
	// This simplifies the actual scaling math later, allowing a fast
	// 8x8-bit multiply and taking the MSB. 'brightness' is a uint8_t,
	// adding 1 here may (intentionally) roll over...so 0 = max brightness
	// (color values are interpreted literally; no scaling), 1 = min
	// brightness (off), 255 = just below max brightness.
	uint8_t newBrightness = b + 1;
	if (newBrightness != brightness) { // Compare against prior value
	    // Brightness has changed -- re-scale existing data in RAM,
	    // This process is potentially "lossy," especially when increasing
	    // brightness. The tight timing in the WS2811/WS2812 code means there
	    // aren't enough free cycles to perform this scaling on the fly as data
	    // is issued. So we make a pass through the existing color data in RAM
	    // and scale it (subsequent graphics commands also work at this
	    // brightness level). If there's a significant step up in brightness,
	    // the limited number of steps (quantization) in the old data will be
	    // quite visible in the re-scaled version. For a non-destructive
	    // change, you'll need to re-render the full strip data. C'est la vie.
	    uint8_t c, *ptr = pixels,
		       oldBrightness = brightness - 1; // De-wrap old brightness value
	    uint16_t scale;
	    if (oldBrightness == 0)
		scale = 0; // Avoid /0
	    else if (b == 255)
		scale = 65535 / oldBrightness;
	    else
		scale = (((uint16_t)newBrightness << 8) - 1) / oldBrightness;
	    for (uint16_t i = 0; i < numLEDs * 3; i++) {
		c = *ptr;
		*ptr++ = (c * scale) >> 8;
	    }
	    brightness = newBrightness;
	}
    }

    /*!
      @brief   Return the number of pixels in an Fake_NeoPixel strip object.
      @return  Pixel count (0 if not set).
    */
    uint16_t numPixels(void) const { return numLEDs; }

    /*!
      @brief   Query the color of a previously-set pixel.
      @param   n  Index of pixel to read (0 = first).
      @return  'Packed' 32-bit RGB or WRGB value. Most significant byte is white
	       (for RGBW pixels) or 0 (for RGB pixels), next is red, then green,
	       and least significant byte is blue.
      @note    If the strip brightness has been changed from the default value
	       of 255, the color read from a pixel may not exactly match what
	       was previously written with one of the setPixelColor() functions.
	       This gets more pronounced at lower brightness levels.
    */
    uint32_t getPixelColor(uint16_t n) const {
	if (n >= numLEDs)
	    return 0; // Out of bounds, return no color.

	uint8_t *p = &pixels[n * 3];
	if (brightness) {
	    // Stored color was decimated by setBrightness(). Returned value
	    // attempts to scale back to an approximation of the original 24-bit
	    // value used when setting the pixel color, but there will always be
	    // some error -- those bits are simply gone. Issue is most
	    // pronounced at low brightness levels.
	    return (((uint32_t)(p[0] << 8) / brightness) << 16) |
		   (((uint32_t)(p[1] << 8) / brightness) << 8) |
		   ((uint32_t)(p[2] << 8) / brightness);
	} else {
	    // No brightness adjustment has been made -- return 'raw' color
	    return ((uint32_t)p[0] << 16) |
		   ((uint32_t)p[1] << 8) |
		   (uint32_t)p[2];
	}
    }

    /*!
      @brief   Convert separate red, green and blue values into a single
	       "packed" 32-bit RGB color.
      @param   r  Red brightness, 0 to 255.
      @param   g  Green brightness, 0 to 255.
      @param   b  Blue brightness, 0 to 255.
      @return  32-bit packed RGB value, which can then be assigned to a
	       variable for later use or passed to the setPixelColor()
	       function. Packed RGB format is predictable, regardless of
	       LED strand color order.
    */
    static uint32_t Color(uint8_t r, uint8_t g, uint8_t b) {
	return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }

private:

protected:
    SDL_Window *window;
    SDL_Renderer *renderer;
    unsigned int width, height;
    uint16_t numLEDs;   ///< Number of RGB LEDs in strip
    uint8_t brightness; ///< Strip brightness 0-255 (stored as +1)
    uint8_t *pixels;    ///< Holds LED color values (3 or 4 bytes each)
};

// The rest of this file is new code by Michael Ditto.  License: GPLv3

static void delay(unsigned int delaytime)
{
    usleep(delaytime * 1000U);
}

static const unsigned int SCREEN_WIDTH=1280, SCREEN_HEIGHT=32;
static const unsigned int NUMPIXELS = 46;
Fake_NeoPixel pixels(NUMPIXELS, SCREEN_WIDTH, SCREEN_HEIGHT);

#define DELAYVAL 50  // frame time (in milliseconds)

void setup()
{
    pixels.begin();
    pixels.setBrightness(160);  // Try to keep the power consumption under 1 A.
    pixels.clear();
}

void loop()
{
    // Simple demo: shift random lights off the end of the string.

    for (int i=NUMPIXELS-1; i>0; --i)
	pixels.setPixelColor(i, pixels.getPixelColor(i-1));
    pixels.setPixelColor(0, rand() & 0xFFFFFF);

    pixels.show();
    delay(DELAYVAL);
}

int main(int argc, char *argv[])
{
    setup();

    SDL_Event e;
    bool quit = false;
    while (!quit)
    {
	while (SDL_PollEvent(&e) != 0)
	    if (e.type == SDL_QUIT)	// User requests quit.
		quit = true;

	loop();
    }

    return 0;
}
