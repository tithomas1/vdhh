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

#import <Foundation/Foundation.h>

typedef enum VMExportFormat {
    ExportFormatNativeVmz,
    ExportFormatVagrantBox,
} VMExportFormat;


@interface VMImportExport : NSObject

- (void) exportVm: (NSString *)vm_name toFile: (NSString *) file format:(VMExportFormat)fmt completion: (void(^)(NSError *error))completionHandler;
- (NSError*) importVmFromVmz: (NSString *)file toFolder: (NSString *) folder withProgress:(NSProgress *)progress;
- (NSError*) importVmFromVmdk: (NSString *)file toFolder: (NSString *) folder withProgress:(NSProgress *)progress;
+ (uint64_t) totalBytesInFiles: (NSArray *)files;
+ (BOOL) extractArchive: (NSString *)arch_file toFolder: (NSString *) folder
           withProgress: (NSProgress *) progress andError: (NSError **)e;
@end
