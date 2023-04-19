#ifndef _RGBCOL_H
#define _RGBCOL_H

enum led_color {
  black,
  red,
  green,
  blue,
  pink
};

struct rgb_color {
  int red;
  int green;
  int blue;
};

struct rgb_pin {
  int red;
  int green;
  int blue;
};

struct RGBLookup {
    enum led_color color;
    struct rgb_color rgb;
};


extern struct rgb_color lookup_color(enum led_color color);
#endif