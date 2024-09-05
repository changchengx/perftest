// SPDX-License-Identifier: GPL-2.0 OR Linux-OpenIB
// Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

#ifndef MLX5_DSA_H
#define MLX5_DSA_H

#include "perftest_parameters.h"
#include "perftest_resources.h"

void *create_and_modify_umr_qp(struct pingpong_context *ctx, struct perftest_parameters *user_param);
int malloc_dsa_buf(struct pingpong_context *ctx, struct perftest_parameters *user_param, int qp_index, int can_init_mem);
int create_umr(struct pingpong_context *ctx, struct perftest_parameters *user_param, int qp_index);
int update_umr(struct pingpong_context *ctx, struct perftest_parameters *user_param, int qp_index);

#endif
