SlowDisk
========

SlowDisk is a kernel module to artifically slow down disk accesses. This is useful for artificially slowing down disks in homogenous clusters where heterogenous disk performance is required. In particular, I have used this in the past for evaluation of systems with artifically slow disks.

Usage
=====

When installing the module, specify the minimum and maximum cycle wait range (i.e. how many cycles to kill between each disk access). By default, if these parameters are not specified, it uses the range [500,1000]. The range is inclusive on both sides. 

```bash
$ sudo insmod slowdisk.ko minWait=200 maxWait=500
```

