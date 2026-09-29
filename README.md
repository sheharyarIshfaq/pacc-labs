# PACC labs

My lab work for UM5IN160, Parallelism and Accelerators for Cluster Computing,
M2 SESI at Sorbonne Université (2026-2027), taught by
[Adrien Cassagne](https://www.lip6.fr/actualite/personnes-fiche.php?ident=P1521).

The course goes from single-core CPU optimisation and SIMD to multithreading,
GPU programming with CUDA and OpenCL, and MPI across nodes. Everything is
measured on [Dalek](https://dalek.proj.lip6.fr), a cluster at LIP6 assembled
from mini-PC nodes with AMD, Intel and Nvidia hardware. The cluster was built
at LIP6 by Adrien Cassagne, Noé Amiot and Manuel Bouyer.

The labs use [EasyPAP](https://gforgeron.gitlab.io/easypap), a C framework for
parallelising computations on 2D grids. The framework and the lab skeletons
belong to the course, so what I keep here is the kernel code I wrote plus my
notes and measurements.

| lab | subject | what I did |
|---|---|---|
| [01](lab01-dalek-slurm) | Cluster environment | Built EasyPAP on macOS and on Dalek, set up SSH through the LIP6 gateway, ran jobs with SLURM |
| [02](lab02-cpu-optims) | Single-core CPU optimisation | Seven hand transformations of a blur filter: 5x at -O0, and still 2.5x faster than the untouched code at -O3 |

## The machine

[Dalek](https://dalek.proj.lip6.fr) is built from consumer hardware rather than
server parts: mini-PCs, laptop SoCs and gaming GPUs, 16 compute nodes plus a
front node, wired with 2.5 GbE. The point is to get cheap access to recent
architectures, including hybrid CPUs with performance and efficiency cores, and
to study energy use on them. The project was funded by the Agence Innovation
Défense and has a paper on HAL and arXiv.

Every node carries a custom board (Node-Conso Modular) that measures power
between the PSU and the components, 1000 to 4000 samples per second, with CPU,
GPU and motherboard read separately. So the consumption figures come from the
hardware and not from a software estimate.

The four compute partitions, all sharing the front node's home directory over
NFS, so one build runs everywhere:

| partition | CPU | GPU |
|---|---|---|
| az4-n4090 | AMD Ryzen 9 7945HX (16x Zen 4) | Nvidia RTX 4090 |
| az4-a7900 | AMD Ryzen 9 7945HX (16x Zen 4) | AMD RX 7900 XTX |
| iml-ia770 | Intel Core Ultra 9 185H | Intel Arc A770 |
| az5-a890m | AMD Ryzen AI 9 HX 370 (Zen 5) | Radeon 890M |

Nodes suspend themselves after 10 minutes with no job on them and are woken by
SLURM through Wake-on-LAN, which is why a pending job often means a machine is
still booting.
