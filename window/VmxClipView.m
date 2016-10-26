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

#import "VmxClipView.h"

@implementation VmxClipView

- (NSRect)constrainBoundsRect:(NSRect)proposedClipViewBoundsRect {
    
    NSRect constrainedClipViewBoundsRect = [super constrainBoundsRect:proposedClipViewBoundsRect];
    
    // Early out if you want to use the default NSClipView behavior.
    //if (self.centersDocumentView == NO) {
      //  return constrainedClipViewBoundsRect;
    //}
    
    NSRect documentViewFrameRect = [self.documentView frame];
    //NSLog(@"documentViewFrameRect: %f %f %f %f", documentViewFrameRect.origin.x, documentViewFrameRect.origin.y, documentViewFrameRect.size.width,documentViewFrameRect.size.height);
    
    // If proposed clip view bounds width is greater than document view frame width, center it horizontally.
    if (proposedClipViewBoundsRect.size.width >= documentViewFrameRect.size.width) {
        // Adjust the proposed origin.x
        constrainedClipViewBoundsRect.origin.x = centeredCoordinateUnitWithProposedContentViewBoundsDimensionAndDocumentViewFrameDimension(proposedClipViewBoundsRect.size.width, documentViewFrameRect.size.width);
    }
    
    // If proposed clip view bounds is hight is greater than document view frame height, center it vertically.
    if (proposedClipViewBoundsRect.size.height >= documentViewFrameRect.size.height) {
        
        // Adjust the proposed origin.y
        constrainedClipViewBoundsRect.origin.y = centeredCoordinateUnitWithProposedContentViewBoundsDimensionAndDocumentViewFrameDimension(proposedClipViewBoundsRect.size.height, documentViewFrameRect.size.height);
    }
     //NSLog(@"constrainedClipViewBoundsRect: %f %f %f %f", constrainedClipViewBoundsRect.origin.x, constrainedClipViewBoundsRect.origin.y, constrainedClipViewBoundsRect.size.width,constrainedClipViewBoundsRect.size.height);

    return constrainedClipViewBoundsRect;
}


CGFloat centeredCoordinateUnitWithProposedContentViewBoundsDimensionAndDocumentViewFrameDimension
                (CGFloat proposedContentViewBoundsDimension,
                CGFloat documentViewFrameDimension)
{
    CGFloat result = roundf( (proposedContentViewBoundsDimension - documentViewFrameDimension) / -2.0F );
    return result;
}

@end
