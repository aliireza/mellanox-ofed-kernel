/* SPDX-License-Identifier: GPL-2.0 OR Linux-OpenIB */
/* Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES. */

#include <linux/netdevice.h>
#include "lag.h"

enum {
	MLX5_LAG_FT_LEVEL_TTC,
	MLX5_LAG_FT_LEVEL_INNER_TTC,
	MLX5_LAG_FT_LEVEL_DEFINER,
};

static int mlx5_lag_create_port_sel_table(struct mlx5_lag *ldev,
					  struct mlx5_lag_definer *lag_definer,
					  u8 port1, u8 port2)
{
	struct mlx5_core_dev *dev = ldev->pf[MLX5_LAG_P1].dev;
	struct mlx5_flow_table_attr ft_attr = {};
	struct mlx5_flow_destination dest = {};
	MLX5_DECLARE_FLOW_ACT(flow_act);
	struct mlx5_flow_namespace *ns;
	int err, i;

	ft_attr.max_fte = MLX5_MAX_PORTS;
	ft_attr.level = MLX5_LAG_FT_LEVEL_DEFINER;

	ns = mlx5_get_flow_namespace(dev, MLX5_FLOW_NAMESPACE_PORT_SEL);
	if (!ns) {
		mlx5_core_warn(dev, "Failed to get port selection namespace\n");
		return -EOPNOTSUPP;
	}

	lag_definer->ft = mlx5_create_flow_table(ns, &ft_attr);
	if (IS_ERR(lag_definer->ft)) {
		mlx5_core_warn(dev, "Failed to create port selection table\n");
		return PTR_ERR(lag_definer->ft);
	}

	lag_definer->fg = mlx5_create_hash_flow_group(lag_definer->ft,
						      lag_definer->definer,
						      0, MLX5_MAX_PORTS);
	if (IS_ERR(lag_definer->fg)) {
		err = PTR_ERR(lag_definer->fg);
		goto destroy_ft;
	}

	dest.type = MLX5_FLOW_DESTINATION_TYPE_UPLINK;
	dest.vport.flags |= MLX5_FLOW_DEST_VPORT_VHCA_ID;
	flow_act.flags |= FLOW_ACT_NO_APPEND;
	for (i = 0; i < MLX5_MAX_PORTS; i++) {
		u8 aff = i == 0 ? port1 : port2;

		dest.vport.vhca_id = MLX5_CAP_GEN(ldev->pf[aff - 1].dev, vhca_id);
		lag_definer->rules[i] = mlx5_add_flow_rules(lag_definer->ft,
							    NULL, &flow_act,
							    &dest, 1);
		if (IS_ERR(lag_definer->rules[i])) {
			err = PTR_ERR(lag_definer->rules[i]);
			while (i--)
				mlx5_del_flow_rules(lag_definer->rules[i]);
			goto destroy_fg;
		}
	}

	return 0;

destroy_fg:
	mlx5_destroy_flow_group(lag_definer->fg);
destroy_ft:
	mlx5_destroy_flow_table(lag_definer->ft);
	return err;
}

static int mlx5_lag_set_definer_inner(u32 *match_definer_mask,
				      enum mlx5_traffic_types tt)
{
	int format_id;
	u8 *ipv6;

	switch (tt) {
	case MLX5_TT_IPV4_UDP:
	case MLX5_TT_IPV4_TCP:
		format_id = 23;
		MLX5_SET_TO_ONES(match_definer_format_23, match_definer_mask,
				 inner_l4_sport);
		MLX5_SET_TO_ONES(match_definer_format_23, match_definer_mask,
				 inner_l4_dport);
		MLX5_SET_TO_ONES(match_definer_format_23, match_definer_mask,
				 inner_ip_src_addr);
		MLX5_SET_TO_ONES(match_definer_format_23, match_definer_mask,
				 inner_ip_dest_addr);
		break;
	case MLX5_TT_IPV4:
		format_id = 23;
		MLX5_SET_TO_ONES(match_definer_format_23, match_definer_mask,
				 inner_l3_type);
		MLX5_SET_TO_ONES(match_definer_format_23, match_definer_mask,
				 inner_dmac_47_16);
		MLX5_SET_TO_ONES(match_definer_format_23, match_definer_mask,
				 inner_dmac_15_0);
		MLX5_SET_TO_ONES(match_definer_format_23, match_definer_mask,
				 inner_smac_47_16);
		MLX5_SET_TO_ONES(match_definer_format_23, match_definer_mask,
				 inner_smac_15_0);
		MLX5_SET_TO_ONES(match_definer_format_23, match_definer_mask,
				 inner_ip_src_addr);
		MLX5_SET_TO_ONES(match_definer_format_23, match_definer_mask,
				 inner_ip_dest_addr);
		break;
	case MLX5_TT_IPV6_TCP:
	case MLX5_TT_IPV6_UDP:
		format_id = 31;
		MLX5_SET_TO_ONES(match_definer_format_31, match_definer_mask,
				 inner_l4_sport);
		MLX5_SET_TO_ONES(match_definer_format_31, match_definer_mask,
				 inner_l4_dport);
		ipv6 = MLX5_ADDR_OF(match_definer_format_31, match_definer_mask,
				    inner_ip_dest_addr);
		memset(ipv6, 0xff, 16);
		ipv6 = MLX5_ADDR_OF(match_definer_format_31, match_definer_mask,
				    inner_ip_src_addr);
		memset(ipv6, 0xff, 16);
		break;
	case MLX5_TT_IPV6:
		format_id = 32;
		ipv6 = MLX5_ADDR_OF(match_definer_format_32, match_definer_mask,
				    inner_ip_dest_addr);
		memset(ipv6, 0xff, 16);
		ipv6 = MLX5_ADDR_OF(match_definer_format_32, match_definer_mask,
				    inner_ip_src_addr);
		memset(ipv6, 0xff, 16);
		MLX5_SET_TO_ONES(match_definer_format_32, match_definer_mask,
				 inner_dmac_47_16);
		MLX5_SET_TO_ONES(match_definer_format_32, match_definer_mask,
				 inner_dmac_15_0);
		MLX5_SET_TO_ONES(match_definer_format_32, match_definer_mask,
				 inner_smac_47_16);
		MLX5_SET_TO_ONES(match_definer_format_32, match_definer_mask,
				 inner_smac_15_0);
		break;
	default:
		format_id = 23;
		MLX5_SET_TO_ONES(match_definer_format_23, match_definer_mask,
				 inner_l3_type);
		MLX5_SET_TO_ONES(match_definer_format_23, match_definer_mask,
				 inner_dmac_47_16);
		MLX5_SET_TO_ONES(match_definer_format_23, match_definer_mask,
				 inner_dmac_15_0);
		MLX5_SET_TO_ONES(match_definer_format_23, match_definer_mask,
				 inner_smac_47_16);
		MLX5_SET_TO_ONES(match_definer_format_23, match_definer_mask,
				 inner_smac_15_0);
		break;
	}

	return format_id;
}

static int mlx5_lag_set_definer(u32 *match_definer_mask,
				enum mlx5_traffic_types tt, bool tunnel,
				enum netdev_lag_hash hash)
{
	int format_id;
	u8 *ipv6;

	if (tunnel)
		return mlx5_lag_set_definer_inner(match_definer_mask, tt);

	switch (tt) {
	case MLX5_TT_IPV4_UDP:
	case MLX5_TT_IPV4_TCP:
		format_id = 22;
		MLX5_SET_TO_ONES(match_definer_format_22, match_definer_mask,
				 outer_l4_sport);
		MLX5_SET_TO_ONES(match_definer_format_22, match_definer_mask,
				 outer_l4_dport);
		MLX5_SET_TO_ONES(match_definer_format_22, match_definer_mask,
				 outer_ip_src_addr);
		MLX5_SET_TO_ONES(match_definer_format_22, match_definer_mask,
				 outer_ip_dest_addr);
		break;
	case MLX5_TT_IPV4:
		format_id = 22;
		MLX5_SET_TO_ONES(match_definer_format_22, match_definer_mask,
				 outer_l3_type);
		MLX5_SET_TO_ONES(match_definer_format_22, match_definer_mask,
				 outer_dmac_47_16);
		MLX5_SET_TO_ONES(match_definer_format_22, match_definer_mask,
				 outer_dmac_15_0);
		MLX5_SET_TO_ONES(match_definer_format_22, match_definer_mask,
				 outer_smac_47_16);
		MLX5_SET_TO_ONES(match_definer_format_22, match_definer_mask,
				 outer_smac_15_0);
		MLX5_SET_TO_ONES(match_definer_format_22, match_definer_mask,
				 outer_ip_src_addr);
		MLX5_SET_TO_ONES(match_definer_format_22, match_definer_mask,
				 outer_ip_dest_addr);
		break;
	case MLX5_TT_IPV6_TCP:
	case MLX5_TT_IPV6_UDP:
		format_id = 29;
		MLX5_SET_TO_ONES(match_definer_format_29, match_definer_mask,
				 outer_l4_sport);
		MLX5_SET_TO_ONES(match_definer_format_29, match_definer_mask,
				 outer_l4_dport);
		ipv6 = MLX5_ADDR_OF(match_definer_format_29, match_definer_mask,
				    outer_ip_dest_addr);
		memset(ipv6, 0xff, 16);
		ipv6 = MLX5_ADDR_OF(match_definer_format_29, match_definer_mask,
				    outer_ip_src_addr);
		memset(ipv6, 0xff, 16);
		break;
	case MLX5_TT_IPV6:
		format_id = 30;
		ipv6 = MLX5_ADDR_OF(match_definer_format_30, match_definer_mask,
				    outer_ip_dest_addr);
		memset(ipv6, 0xff, 16);
		ipv6 = MLX5_ADDR_OF(match_definer_format_30, match_definer_mask,
				    outer_ip_src_addr);
		memset(ipv6, 0xff, 16);
		MLX5_SET_TO_ONES(match_definer_format_30, match_definer_mask,
				 outer_dmac_47_16);
		MLX5_SET_TO_ONES(match_definer_format_30, match_definer_mask,
				 outer_dmac_15_0);
		MLX5_SET_TO_ONES(match_definer_format_30, match_definer_mask,
				 outer_smac_47_16);
		MLX5_SET_TO_ONES(match_definer_format_30, match_definer_mask,
				 outer_smac_15_0);
		break;
	default:
		format_id = 0;
		MLX5_SET_TO_ONES(match_definer_format_0, match_definer_mask,
				 outer_smac_47_16);
		MLX5_SET_TO_ONES(match_definer_format_0, match_definer_mask,
				 outer_smac_15_0);
		MLX5_SET_TO_ONES(match_definer_format_0, match_definer_mask,
				 outer_ethertype);
		MLX5_SET_TO_ONES(match_definer_format_0, match_definer_mask,
				 outer_dmac_47_16);
		MLX5_SET_TO_ONES(match_definer_format_0, match_definer_mask,
				 outer_dmac_15_0);
		break;
	}

	return format_id;
}

static struct mlx5_lag_definer *
mlx5_lag_create_definer(struct mlx5_lag *ldev, enum netdev_lag_hash hash,
			enum mlx5_traffic_types tt, bool tunnel, u8 port1,
			u8 port2)
{
	struct mlx5_core_dev *dev = ldev->pf[MLX5_LAG_P1].dev;
	struct mlx5_lag_definer *lag_definer;
	u32 *match_definer_mask;
	int format_id, err;

	lag_definer = kzalloc(sizeof(*lag_definer), GFP_KERNEL);
	if (!lag_definer)
		return ERR_PTR(ENOMEM);

	match_definer_mask = kvzalloc(MLX5_DEFINER_MASK_SZ, GFP_KERNEL);
	if (!match_definer_mask) {
		err = -ENOMEM;
		goto free_lag_definer;
	}

	format_id = mlx5_lag_set_definer(match_definer_mask, tt, tunnel, hash);
	lag_definer->definer =
		mlx5_create_match_definer(dev, MLX5_FLOW_NAMESPACE_PORT_SEL,
					  format_id, match_definer_mask);
	if (IS_ERR(lag_definer->definer)) {
		err = PTR_ERR(lag_definer->definer);
		goto free_mask;
	}

	err = mlx5_lag_create_port_sel_table(ldev, lag_definer, port1, port2);
	if (err)
		goto destroy_match_definer;

	kvfree(match_definer_mask);

	return lag_definer;

destroy_match_definer:
	mlx5_destroy_match_definer(dev, lag_definer->definer);
free_mask:
	kvfree(match_definer_mask);
free_lag_definer:
	kfree(lag_definer);
	return ERR_PTR(err);
}

static void mlx5_lag_destroy_definer(struct mlx5_lag *ldev,
				     struct mlx5_lag_definer *lag_definer)
{
	struct mlx5_core_dev *dev = ldev->pf[MLX5_LAG_P1].dev;
	int i;

	for (i = 0; i < MLX5_MAX_PORTS; i++)
		mlx5_del_flow_rules(lag_definer->rules[i]);
	mlx5_destroy_flow_group(lag_definer->fg);
	mlx5_destroy_flow_table(lag_definer->ft);
	mlx5_destroy_match_definer(dev, lag_definer->definer);
	kfree(lag_definer);
}

static void mlx5_lag_destroy_definers(struct mlx5_lag *ldev)
{
	struct mlx5_lag_steering *steering = &ldev->steering;
	int tt;

	for_each_set_bit(tt, steering->tt_map, MLX5_NUM_TT) {
		if (steering->outer.definers[tt])
			mlx5_lag_destroy_definer(ldev,
						 steering->outer.definers[tt]);
		if (steering->inner.definers[tt])
			mlx5_lag_destroy_definer(ldev,
						 steering->inner.definers[tt]);
	}
}

static int mlx5_lag_create_definers(struct mlx5_lag *ldev,
				    struct lag_tracker *tracker)
{
	struct mlx5_lag_steering *steering = &ldev->steering;
	struct mlx5_lag_definer *lag_definer;
	u8 port1, port2;
	int tt, err;

	mlx5_lag_infer_tx_affinity_mapping(tracker, &port1, &port2);
	for_each_set_bit(tt, steering->tt_map, MLX5_NUM_TT) {
		lag_definer = mlx5_lag_create_definer(ldev, tracker->hash_type,
						      tt, false, port1, port2);
		if (IS_ERR(lag_definer)) {
			err = PTR_ERR(lag_definer);
			goto destroy_definers;
		}
		steering->outer.definers[tt] = lag_definer;

		if (!steering->tunnel)
			continue;

		lag_definer =
			mlx5_lag_create_definer(ldev, tracker->hash_type, tt,
						true, port1, port2);
		if (IS_ERR(lag_definer)) {
			err = PTR_ERR(lag_definer);
			goto destroy_definers;
		}
		steering->inner.definers[tt] = lag_definer;
	}

	return 0;

destroy_definers:
	mlx5_lag_destroy_definers(ldev);
	return err;
}

static void set_tt_map(struct mlx5_lag_steering *steering,
		       enum netdev_lag_hash hash)
{
	steering->tunnel = false;

	switch (hash) {
	case NETDEV_LAG_HASH_E34:
		steering->tunnel = true;
		fallthrough;
	case NETDEV_LAG_HASH_L34:
		set_bit(MLX5_TT_IPV4_TCP, steering->tt_map);
		set_bit(MLX5_TT_IPV4_UDP, steering->tt_map);
		set_bit(MLX5_TT_IPV6_TCP, steering->tt_map);
		set_bit(MLX5_TT_IPV6_UDP, steering->tt_map);
		set_bit(MLX5_TT_IPV4, steering->tt_map);
		set_bit(MLX5_TT_IPV6, steering->tt_map);
		set_bit(MLX5_TT_ANY, steering->tt_map);
		break;
	case NETDEV_LAG_HASH_E23:
		steering->tunnel = true;
		fallthrough;
	case NETDEV_LAG_HASH_L23:
		set_bit(MLX5_TT_IPV4, steering->tt_map);
		set_bit(MLX5_TT_IPV6, steering->tt_map);
		set_bit(MLX5_TT_ANY, steering->tt_map);
		break;
	default:
		set_bit(MLX5_TT_ANY, steering->tt_map);
		break;
	}
}

static void mlx5_lag_set_inner_ttc_params(struct mlx5_lag *ldev,
					  struct ttc_params *ttc_params)
{
	struct mlx5_core_dev *dev = ldev->pf[MLX5_LAG_P1].dev;
	struct mlx5_lag_steering *steering = &ldev->steering;
	struct mlx5_flow_table_attr *ft_attr;
	int tt;

	ttc_params->ns = mlx5_get_flow_namespace(dev,
						 MLX5_FLOW_NAMESPACE_PORT_SEL);
	ft_attr = &ttc_params->ft_attr;
	ft_attr->level = MLX5_LAG_FT_LEVEL_INNER_TTC;

	for_each_set_bit(tt, steering->tt_map, MLX5_NUM_TT) {
		ttc_params->dests[tt].type =
			MLX5_FLOW_DESTINATION_TYPE_FLOW_TABLE;
		ttc_params->dests[tt].ft = steering->inner.definers[tt]->ft;
		set_bit(tt, ttc_params->dests_valid);
	}
}

static void mlx5_lag_set_outer_ttc_params(struct mlx5_lag *ldev,
					  struct ttc_params *ttc_params)
{
	struct mlx5_core_dev *dev = ldev->pf[MLX5_LAG_P1].dev;
	struct mlx5_lag_steering *steering = &ldev->steering;
	struct mlx5_flow_table_attr *ft_attr;
	int tt;

	ttc_params->ns = mlx5_get_flow_namespace(dev,
						 MLX5_FLOW_NAMESPACE_PORT_SEL);
	ft_attr = &ttc_params->ft_attr;
	ft_attr->level = MLX5_LAG_FT_LEVEL_TTC;

	for_each_set_bit(tt, steering->tt_map, MLX5_NUM_TT) {
		ttc_params->dests[tt].type =
			MLX5_FLOW_DESTINATION_TYPE_FLOW_TABLE;
		ttc_params->dests[tt].ft = steering->outer.definers[tt]->ft;
		set_bit(tt, ttc_params->dests_valid);
	}

	ttc_params->inner_ttc = steering->tunnel;
	if (!steering->tunnel)
		return;

	for (tt = 0; tt < MLX5_NUM_TUNNEL_TT; tt++) {
		ttc_params->tunnel_dests[tt].type =
			MLX5_FLOW_DESTINATION_TYPE_FLOW_TABLE;
		ttc_params->tunnel_dests[tt].ft =
			mlx5_get_ttc_flow_table(steering->inner.ttc);
		set_bit(tt, ttc_params->tunnel_dests_valid);
	}
}

static int mlx5_lag_create_ttc_table(struct mlx5_lag *ldev)
{
	struct mlx5_core_dev *dev = ldev->pf[MLX5_LAG_P1].dev;
	struct mlx5_lag_steering *steering = &ldev->steering;
	struct ttc_params ttc_params = {};

	mlx5_lag_set_outer_ttc_params(ldev, &ttc_params);
	steering->outer.ttc = mlx5_create_ttc_table(dev, &ttc_params);
	if (IS_ERR(steering->outer.ttc))
		return PTR_ERR(steering->outer.ttc);

	return 0;
}

static int mlx5_lag_create_inner_ttc_table(struct mlx5_lag *ldev)
{
	struct mlx5_core_dev *dev = ldev->pf[MLX5_LAG_P1].dev;
	struct mlx5_lag_steering *steering = &ldev->steering;
	struct ttc_params ttc_params = {};

	mlx5_lag_set_inner_ttc_params(ldev, &ttc_params);
	steering->inner.ttc = mlx5_create_ttc_table(dev, &ttc_params);
	if (IS_ERR(steering->inner.ttc))
		return PTR_ERR(steering->inner.ttc);

	return 0;
}

int mlx5_lag_create_port_selection(struct mlx5_lag *ldev,
				   struct lag_tracker *tracker)
{
	struct mlx5_lag_steering *steering = &ldev->steering;
	int err;

	set_tt_map(steering, tracker->hash_type);
	err = mlx5_lag_create_definers(ldev, tracker);
	if (err)
		return err;

	if (steering->tunnel) {
		err = mlx5_lag_create_inner_ttc_table(ldev);
		if (err)
			goto destroy_definers;
	}

	err = mlx5_lag_create_ttc_table(ldev);
	if (err)
		goto destroy_inner;

	return 0;

destroy_inner:
	if (steering->tunnel)
		mlx5_destroy_ttc_table(steering->inner.ttc);
destroy_definers:
	mlx5_lag_destroy_definers(ldev);
	return err;
}

static int
mlx5_lag_modify_definers_destinations(struct mlx5_lag *ldev,
				      struct mlx5_lag_definer **definers,
				      u8 port1, u8 port2)
{
	struct mlx5_lag_steering *steering = &ldev->steering;
	struct mlx5_flow_destination dest = {};
	int err;
	int tt;

	dest.type = MLX5_FLOW_DESTINATION_TYPE_UPLINK;
	dest.vport.flags |= MLX5_FLOW_DEST_VPORT_VHCA_ID;

	for_each_set_bit(tt, steering->tt_map, MLX5_NUM_TT) {
		struct mlx5_flow_handle **rules = definers[tt]->rules;

		if (ldev->v2p_map[MLX5_LAG_P1] != port1) {
			dest.vport.vhca_id =
				MLX5_CAP_GEN(ldev->pf[port1 - 1].dev, vhca_id);
			err = mlx5_modify_rule_destination(rules[MLX5_LAG_P1],
							   &dest, NULL);
			if (err)
				return err;
		}

		if (ldev->v2p_map[MLX5_LAG_P2] != port2) {
			dest.vport.vhca_id =
				MLX5_CAP_GEN(ldev->pf[port2 - 1].dev, vhca_id);
			err = mlx5_modify_rule_destination(rules[MLX5_LAG_P2],
							   &dest, NULL);
			if (err)
				return err;
		}
	}

	return 0;
}

int mlx5_lag_modify_port_selection(struct mlx5_lag *ldev, u8 port1, u8 port2)
{
	struct mlx5_lag_steering *steering = &ldev->steering;
	int err;

	err = mlx5_lag_modify_definers_destinations(ldev,
						    steering->outer.definers,
						    port1, port2);
	if (err)
		return err;

	if (!steering->tunnel)
		return 0;

	return mlx5_lag_modify_definers_destinations(ldev,
						     steering->inner.definers,
						     port1, port2);
}

void mlx5_lag_destroy_port_selection(struct mlx5_lag *ldev)
{
	struct mlx5_lag_steering *steering = &ldev->steering;

	mlx5_destroy_ttc_table(steering->outer.ttc);
	if (steering->tunnel)
		mlx5_destroy_ttc_table(steering->inner.ttc);
	mlx5_lag_destroy_definers(ldev);
	memset(steering, 0, sizeof(*steering));
}
