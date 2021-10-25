# Mellanox OFED Kernel

This repo contains the source code for `mlnx-ofed-kernel_5.4.orig.tar.gz`.

## Building Package

If you change the original source code, you have to commit the changes as a patch and then build a new package. You can run the following commands:

```bash
dpkg-source --commit
dpkg-buildpackage -us -uc
cd ../
sudo dpkg -i mlnx-ofed-kernel-dkms*
```

## Testing

To test the new package, you can restart Mellanox kernel module, as follows:

```bash
sudo /etc/init.d/openibd restart
```

You can check the driver messages via `sudo dmesg`.

## More info

Check [here](https://insujang.github.io/2020-01-25/building-mellanox-ofed-from-source/) for more details. 
