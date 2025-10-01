#pragma once
//
//  h264_parser.h
//  H264_Parser
//
//  Created by Ternence on 2019/3/22.
//  Copyright © 2019 Ternence. All rights reserved.
//

#ifndef h264_parser_h
#define h264_parser_h

#include <stdio.h>
#include "NaluType.h"

#ifdef __cplusplus
extern "C" {
#endif

int simplest_h264_parser(const char *url, void(*out_nalu)(char * buffer, int size, NaluType type));
//int simplest_h264_parser(const char *url);

#ifdef __cplusplus
}
#endif

#endif /* h264_parser_h */
