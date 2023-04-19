#include "RGBColor.h"

const struct RGBLookup rgb_lookup[5] = {
    {black, {0, 0, 0}},
    {red, {255, 0, 0}},
    {green, {0, 255, 0}},
    {blue, {0, 0, 255}},
    {pink, {221, 8, 143}},
};

struct rgb_color lookup_color(enum led_color color) {
    for (int i = 0; i < sizeof(rgb_lookup) / sizeof(struct RGBLookup); i++) {
        if (rgb_lookup[i].color == color) {
            return rgb_lookup[i].rgb;
        }
    }
    // return black if the color is not found in the lookup table
    return (struct rgb_color){0, 0, 0};
}