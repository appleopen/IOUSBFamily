/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.2 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.  
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#import <Foundation/Foundation.h>
#import "authorization.h"

@interface systemCommands : NSObject {
    
}

// returns the unparsed output from kmodstat command
+(NSString *)kmodstatWithAuth:(BOOL)needsAuth;

// greps the string "inputString". greparguments is nil terminated array, and uses same arguments as command line grep
+(NSString *)grep:(NSString *)inputString arguments:(NSArray *)greparguments;

// pipes the string "inputString" into the awk command. awkarguments is nil terminated array, and uses same arguments as command line awk
+(NSString *)awk:(NSString *)inputString arguments:(NSArray *)awkarguments;

@end