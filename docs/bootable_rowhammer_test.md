
# Bootable version of rowhammer_test

As noted in the README.md file, running `rowhammer_test` on a live
system is somewhat risky, because there's a chance it can corrupt
important data.  You can reduce the risk by booting `rowhammer_test`
as the only process on a system.

The script `make_initrd.sh` will create a Linux initrd image that can
be run under Linux at boot time to run `rowhammer_test`.


## Usage

```
./make_initrd.sh
sudo cp -v out/rowhammer_test_initrd.gz /boot/
```

Then, assuming your machine uses the GRUB bootloader:

* Reboot the machine.
* As the machine starts up, hold down `Escape` to tell GRUB to display
  its boot menu.
* Press `E` to edit the default boot script.
* The last line should be something like `initrd /initrd.img-<version>`.
  Replace that with `initrd /rowhammer_test_initrd.gz`.
* Press `Ctrl-X` to run the boot script.
* Linux should now start up and run `rowhammer_test`.
