// SPDX-License-Identifier: GPL-2.0 OR Linux-OpenIB
/* Copyright (c) 2020 Mellanox Technologies Ltd */

#include <linux/mlx5/driver.h>
#include <linux/mlx5/device.h>
#include "mlx5_core.h"
#include "dev.h"
#include "devlink.h"
#include "cfg_driver.h"

static int mlx5_sf_dev_probe(struct auxiliary_device *adev, const struct auxiliary_device_id *id)
{
	struct mlx5_sf_dev *sf_dev = container_of(adev, struct mlx5_sf_dev, adev);
	struct mlx5_core_dev *mdev;
	struct devlink *devlink;
	int err;

	devlink = mlx5_devlink_alloc();
	if (!devlink)
		return -ENOMEM;

	mdev = devlink_priv(devlink);
	mdev->device = &adev->dev;
	mdev->pdev = sf_dev->parent_mdev->pdev;
	mdev->bar_addr = sf_dev->bar_base_addr;
	mdev->iseg_base = sf_dev->bar_base_addr;
	mdev->coredev_type = MLX5_COREDEV_SF;
	mdev->priv.parent_mdev = sf_dev->parent_mdev;
	mdev->priv.adev_idx = adev->id;
	sf_dev->mdev = mdev;

	err = mlx5_mdev_init(mdev, MLX5_DEFAULT_PROF);
	if (err) {
		mlx5_core_warn(mdev, "mlx5_mdev_init on err=%d\n", err);
		goto mdev_err;
	}

#ifdef CONFIG_MLX5_SF_CFG
	mdev->disable_en = sf_dev->disable_netdev;
	mdev->disable_fc = sf_dev->disable_fc;
	mdev->cmpl_eq_depth = sf_dev->cmpl_eq_depth;
	mdev->async_eq_depth = sf_dev->async_eq_depth;
	mdev->max_cmpl_eq_count = sf_dev->max_cmpl_eqs;
#endif

	mdev->iseg = ioremap(mdev->iseg_base, sizeof(*mdev->iseg));
	if (!mdev->iseg) {
		mlx5_core_warn(mdev, "remap error\n");
		goto remap_err;
	}

	err = mlx5_load_one(mdev, true);
	if (err) {
		mlx5_core_warn(mdev, "mlx5_load_one err=%d\n", err);
		goto load_one_err;
	}
	devlink_reload_enable(devlink);
	return 0;

load_one_err:
	iounmap(mdev->iseg);
remap_err:
	mlx5_mdev_uninit(mdev);
mdev_err:
	mlx5_devlink_free(devlink);
	return err;
}

static void mlx5_sf_dev_remove(struct auxiliary_device *adev)
{
	struct mlx5_sf_dev *sf_dev = container_of(adev, struct mlx5_sf_dev, adev);
	struct devlink *devlink;

	devlink = priv_to_devlink(sf_dev->mdev);
	devlink_reload_disable(devlink);
	mlx5_unload_one(sf_dev->mdev, true);

	/* health work might still be active, and it needs pci bar in
	 * order to know the NIC state. Therefore, drain the health WQ
	 * before removing the pci bars
	 */
	mlx5_drain_health_wq(sf_dev->mdev);
	iounmap(sf_dev->mdev->iseg);
	mlx5_mdev_uninit(sf_dev->mdev);
	mlx5_devlink_free(devlink);
}

static void mlx5_sf_dev_shutdown(struct auxiliary_device *adev)
{
	struct mlx5_sf_dev *sf_dev = container_of(adev, struct mlx5_sf_dev, adev);

	mlx5_unload_one(sf_dev->mdev, false);
}

static const struct auxiliary_device_id mlx5_sf_dev_id_table[] = {
	{ .name = MLX5_ADEV_NAME "." MLX5_SF_DEV_ID_NAME, },
	{ },
};

MODULE_DEVICE_TABLE(auxiliary, mlx5_sf_dev_id_table);

static struct auxiliary_driver mlx5_sf_driver = {
	.name = MLX5_SF_DEV_ID_NAME,
	.probe = mlx5_sf_dev_probe,
	.remove = mlx5_sf_dev_remove,
	.shutdown = mlx5_sf_dev_shutdown,
	.id_table = mlx5_sf_dev_id_table,
};

int mlx5_sf_driver_register(void)
{
	int err;

	err = mlx5_sf_cfg_driver_register();
	if (err)
		return err;

	err = auxiliary_driver_register(&mlx5_sf_driver);
	if (err)
		goto err;
	return 0;

err:
	mlx5_sf_cfg_driver_unregister();
	return err;
}

void mlx5_sf_driver_unregister(void)
{
	auxiliary_driver_unregister(&mlx5_sf_driver);
	mlx5_sf_cfg_driver_unregister();
}
