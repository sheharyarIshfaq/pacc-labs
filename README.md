# PACC labs

My lab work for UM5IN160, Parallelism and Accelerators for Cluster Computing,
M2 SESI at Sorbonne Université (2026-2027), taught by Adrien Cassagne.

The course goes from single-core CPU optimisation and SIMD to multithreading,
GPU programming with CUDA and OpenCL, and MPI across nodes. Everything is
measured on [Dalek](https://dalek.proj.lip6.fr), a cluster at LIP6 assembled
from mini-PC nodes with AMD, Intel and Nvidia hardware.

The labs use [EasyPAP](https://gforgeron.gitlab.io/easypap), a C framework for
parallelising computations on 2D grids. The framework and the lab skeletons
belong to the course, so what I keep here is the kernel code I wrote plus my
notes and measurements.

| lab | subject | what I did |
|---|---|---|
| [01](lab01-dalek-slurm) | Cluster environment | Built EasyPAP on macOS and on Dalek, set up SSH through the LIP6 gateway, ran jobs with SLURM |
| [02](lab02-cpu-optims) | Single-core CPU optimisation | Seven hand transformations of a blur filter: 5x at -O0, and still 2.5x faster than the untouched code at -O3 |

## The machine

Dalek's compute partitions. The front node shares its home directory with all
of them over NFS, so one build runs everywhere.

| partition | CPU | GPU |
|---|---|---|
| az4-n4090 | AMD Ryzen 9 7945HX (16x Zen 4) | Nvidia RTX 4090 |
| az4-a7900 | AMD Ryzen 9 7945HX (16x Zen 4) | AMD RX 7900 XTX |
| iml-ia770 | Intel Core Ultra 9 185H | Intel Arc A770 |
| az5-a890m | AMD Ryzen AI 9 HX 370 (Zen 5) | Radeon 890M |
