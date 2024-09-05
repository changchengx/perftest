// SPDX-License-Identifier: GPL-2.0 OR Linux-OpenIB
// Copyright (c) 2024, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <infiniband/mlx5dv.h>
#include "mlx5_dsa.h"

#define IB_ACCESS_FLAGS        (IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ)

static int rc_qp_loopback_connect(struct ibv_qp *qp) {
	int res = 0;

	struct ibv_port_attr port_attr;
	if (ibv_query_port(qp->context, 1, &port_attr)) {
		fprintf(stderr, "Couldn't get port attr\n");
		return -1;
	}

	union ibv_gid gid;
	if (ibv_query_gid(qp->context, 1, 3, &gid)) {
		fprintf(stderr, "can't read sgid of index %d, strerror=%s\n", 3, strerror(errno));
		return -1;
	}

	struct ibv_qp_attr qp_attr = {};
	qp_attr.qp_state = IBV_QPS_INIT;
	qp_attr.pkey_index = 0;
	qp_attr.port_num = 1;
	qp_attr.qp_access_flags = 0;

	if (ibv_modify_qp(qp, &qp_attr, IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS)) {
		fprintf(stderr, "Failed to INIT the UMR QP\n");
		return -1;
	}

	memset((void *)&qp_attr, 0, sizeof(qp_attr));
	qp_attr.qp_state = IBV_QPS_RTR;
	qp_attr.path_mtu = IBV_MTU_1024;
	qp_attr.dest_qp_num = qp->qp_num;
	qp_attr.rq_psn = 0;
	qp_attr.min_rnr_timer = 20;
	qp_attr.max_dest_rd_atomic = 1;
	qp_attr.ah_attr.is_global = 1;
	qp_attr.ah_attr.dlid = port_attr.lid;
	qp_attr.ah_attr.sl = 0;
	qp_attr.ah_attr.src_path_bits = 0;
	qp_attr.ah_attr.port_num = 1;
	qp_attr.ah_attr.grh.hop_limit = 1;
	qp_attr.ah_attr.grh.dgid = gid;
	qp_attr.ah_attr.grh.sgid_index = 3;
	res = ibv_modify_qp(qp, &qp_attr, IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN | IBV_QP_RQ_PSN |
					IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER);
	if (res) {
		fprintf(stderr, "Failed to modify QP to RTR. reason: %d\n", res);
		return -1;
	}

	qp_attr.qp_state = IBV_QPS_RTS;
	qp_attr.timeout = 10;
	qp_attr.retry_cnt = 7;
	qp_attr.rnr_retry = 7;
	qp_attr.sq_psn = 0;
	qp_attr.max_rd_atomic = 1;
	res = ibv_modify_qp(qp, &qp_attr, IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC);
	if (res) {
		fprintf(stderr, "Failed to modify QP to RTS. reason: %d\n", res);
		return -1;
	}

	return 0;
}

void *create_and_modify_umr_qp(struct pingpong_context *ctx, struct perftest_parameters *user_param)
{
	struct ibv_qp *umr_qp;

	struct mlx5dv_qp_init_attr mlx5_qp_attr = {};
	struct ibv_qp_init_attr_ex init_attr_ex = {};


	mlx5_qp_attr.comp_mask = MLX5DV_QP_INIT_ATTR_MASK_SEND_OPS_FLAGS;
	mlx5_qp_attr.send_ops_flags = MLX5DV_QP_EX_WITH_MR_LIST;

	mlx5_qp_attr.create_flags = MLX5DV_QP_CREATE_TIR_ALLOW_SELF_LOOPBACK_UC;
	mlx5_qp_attr.comp_mask |= IBV_QP_INIT_ATTR_CREATE_FLAGS;

	init_attr_ex.cap.max_inline_data = 256;

	init_attr_ex.cap.max_send_wr = 1;
	init_attr_ex.cap.max_send_sge = 1;

	init_attr_ex.cap.max_recv_wr = 0;
	init_attr_ex.cap.max_recv_sge = 0;

	init_attr_ex.send_cq = ctx->umr_cq;
	init_attr_ex.recv_cq = ctx->umr_cq;

	init_attr_ex.qp_type = IBV_QPT_RC;

	init_attr_ex.pd = ctx->pd;
	init_attr_ex.send_ops_flags = IBV_QP_EX_WITH_SEND;
	init_attr_ex.comp_mask = IBV_QP_INIT_ATTR_PD | IBV_QP_INIT_ATTR_SEND_OPS_FLAGS;

	umr_qp = mlx5dv_create_qp(ctx->context, &init_attr_ex, &mlx5_qp_attr);
	if (!umr_qp) {
		return NULL;
	}

	if (rc_qp_loopback_connect(umr_qp)) {
		ibv_destroy_qp(umr_qp);
		umr_qp = NULL;
		printf("UMR QP wasn't modified to RTS\n");
	}

	return umr_qp;
}
