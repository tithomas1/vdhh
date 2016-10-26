//
//  vsystem.h
//  vlaunch
//
//  Created by Boris Remizov on 02/10/16.
//  Copyright © 2016 Veertu. All rights reserved.
//

#ifndef vsystem_h
#define vsystem_h

/// \brief communication descriptors [rd, wr] for vlaunch system
extern int vlaunchfd[2];

/// \brief launch external command via vlaunch system
int vsystem(const char* command, int wait);

#endif /* vsystem_h */
