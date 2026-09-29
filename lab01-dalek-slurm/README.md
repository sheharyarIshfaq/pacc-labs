# Lab 1: EasyPAP and the Dalek cluster

Setting up the environment for the rest of the course. Build EasyPAP on my
laptop and on the cluster, get through the LIP6 gateway, and run jobs with
SLURM.

## Building EasyPAP

On a laptop the dependencies have to be installed by hand:

    cmake -S . -B build -DEXTERNAL_EASYTOOLS=OFF
    cmake --build build --parallel
    ./run -k spin

On my Mac (Apple Silicon) that meant `cmake`, `pkg-config`, `sdl2`, `cglm`,
`hwloc`, `flex` and `libomp` from Homebrew, and two extra flags. FxT, the
tracing library, is not packaged anywhere I could find, so I turned tracing off
with `-DENABLE_TRACE=OFF`. Apple's clang has no OpenMP, so CMake needs
`-DOpenMP_ROOT=$(brew --prefix libomp)` to find Homebrew's copy.

The one that cost me the most time had nothing to do with EasyPAP: an old Intel
Homebrew in `/usr/local` was shadowing the arm64 binaries, so `cmake` died with
"bad CPU type in executable" and later VS Code refused to open a remote folder
for the same reason with its own `ssh`. `file $(which -a cmake)` is worth
running before believing anything else.

On Dalek the components are prebuilt and come from a module:

    module load easytools
    cmake -S . -B build -DEXTERNAL_EASYTOOLS=ON
    cmake --build build --parallel

`module load easytools` has to be run in every new shell. Without it the binary
starts and immediately fails with `libezm.so: cannot open shared object file`.

## Connecting

Dalek sits behind an SSH gateway, so `~/.ssh/config` does the hop for me:

    Host gateway.lip6
        User <login>
        HostName ssh1.dalek.proj.lip6.fr

    Host front.dalek.lip6
        User <login>
        HostName front.dalek.proj.lip6.fr
        ProxyJump gateway.lip6

The cluster documentation also gives per-node rules that bundle a
`RemoteCommand`, so `ssh az4-a7900-2.dalek.lip6.srun` goes to the front node,
asks SLURM for that compute node and leaves me in a shell on it. Handy, as long
as you remember it only works from the laptop: the config lives there, not on
the cluster.

## Running jobs

The home directory is on NFS, so a binary built on the front node runs on any
compute node without copying anything. Compiling happens on the front node,
timing never does.

    sinfo                                    # partitions and node states
    salloc -p az4-mixed -N 1 -t 00:05:00     # interactive allocation
    srun -p az4-mixed -N 1 -t 00:03:00 ./run -k spin -i 100 -n
    sbatch job.sh
    squeue -u $USER

Two things I would tell myself before starting:

Nodes are powered down after 10 minutes of inactivity and take up to 90 seconds
to wake up, so a job sitting in PD for a while is normal.

My QoS (`pacc26-qos`) allows 2 nodes, 4 CPUs and 5 minutes per job. Asking for
20 minutes is not rejected. The job just stays pending forever with reason
`QOSMaxWallDurationPerJobLimit` in `squeue`, and I spent a while blaming the
cluster before reading that column.

## First measurement

The `spin` kernel, 100 iterations, sequential variant:

| machine | time |
|---|---|
| front node, Core i9-13900H, shared with everyone | 2541 ms |
| az4-a7900-2, Ryzen 9 7945HX, reserved through SLURM | 1752 ms |

The front node is busy with everyone else's compilations, which is why its
numbers are worthless for anything but checking that the build works.
