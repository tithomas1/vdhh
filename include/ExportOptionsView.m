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

#import "ExportOptionsView.h"

@interface ExportOptionsView()

@property (weak, nonatomic) IBOutlet NSPopUpButton *exportMenu;

@end

@implementation ExportOptionsView

@synthesize exportFormat = _exportFormat;

- (void)drawRect:(NSRect)dirtyRect {
    [super drawRect:dirtyRect];
    
    // Drawing code here.
}

- (VMExportFormat) exportFormat
{
    return (VMExportFormat)[self.exportMenu indexOfSelectedItem];
}

- (void) setExportFormat: (VMExportFormat) fmt
{
    return [self.exportMenu selectItemAtIndex: fmt];
}

- (IBAction) selectFormat:(id)sender
{
    NSString *nameFieldString = self.savePanel.nameFieldStringValue;
    NSString *trimmedNameFieldString = [nameFieldString stringByDeletingPathExtension];
    NSString *ext = @"box";

    if ([self exportFormat] == ExportFormatNativeVmz)
        ext = @"vmx";

    NSString *nameFieldStringWithExt = [NSString stringWithFormat:@"%@.%@", trimmedNameFieldString, ext];
    [[self savePanel] setNameFieldStringValue:nameFieldStringWithExt];
    [[self savePanel] setAllowedFileTypes:@[ext]];
}


@end
