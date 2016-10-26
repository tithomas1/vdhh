/*
 * Copyright (C) 2016 Veertu Inc,
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 or
 * (at your option) version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#import "NSColor+RGB.h"

@implementation NSColor (RGB)

+ (NSColor*) colorFromRGB: (int) rgb
{
    float r = (rgb >> 16) & 0xff;
    float g = (rgb >> 8) & 0xff;
    float b = rgb & 0xff;
    return [NSColor colorWithCalibratedRed:(r/255.0f) green:(g/255.0f) blue:(b/255.0f) alpha:1.0];
}

+ (NSColor*) colorFromRGB: (int) rgb alpha: (float) a
{
    float r = (rgb >> 16) & 0xff;
    float g = (rgb >> 8) & 0xff;
    float b = rgb & 0xff;
    return [NSColor colorWithCalibratedRed:(r/255.0f) green:(g/255.0f) blue:(b/255.0f) alpha:a];
}

@end
