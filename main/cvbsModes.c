#include "cvbsModes.h"

const struct cvbsMode MODE_720x480x30NTSC = {31, 361, 61, 63, 10, 35, 3579545, 720, 20, 525, 480, 1, 1, 13333333, 0}; // 720 (timing is off on this one)
const struct cvbsMode MODE_960x480x30NTSC = {43, 500, 84, 87, 13, 52, 3579545, 977, 28, 525, 480, 1, 1, 18461538, 0}; // 960H
const struct cvbsMode MODE_FractionalDivi = {43, 500, 84, 87, 13, 52, 3579545, 977, 28, 585, 540, 1, 1, 17897725, 0}; // using fractional divider for testing
const struct cvbsMode MODE_960x540x30NTSC = {43, 500, 84, 87, 13, 52, 3579545, 977, 28, 585, 540, 1, 1, 18461538, 0}; // 960x540
const struct cvbsMode MODE_21MHZNTSC = {50, 591, 101, 103, 13, 62, 3579545, 1156, 30, 585, 540, 1, 1, 21818181, 0}; // 21mhz version for closest multiple of NTSC color carrier
