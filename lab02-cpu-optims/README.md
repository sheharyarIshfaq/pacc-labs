# Lab 2: single-core CPU optimisation

Optimising a 3x3 blur filter by hand and measuring every step on Dalek. The
kernel replaces each pixel with the average of itself and its 8 neighbours,
over a 1024x1024 image, repeated 100 times.

Only [`blur.c`](blur.c) is mine. It plugs into the EasyPAP framework.

## How it was measured

Sections 1 to 7 are compiled at `-O0`, so that the compiler's own work does not
hide mine. Each version is a separate `do_tile` function picked at run time:

    ./run -k blur --load-image img/1024.png -i 100 -v seq -wt optim6 -n --show-hash

`--show-hash` prints a SHA256 of the resulting image, which is how I checked
that a faster version still computes the same thing. All timings come from
compute nodes. After section 7 I pinned them to `az4-a7900-2`, because the
`az4-mixed` partition kept giving me two distinct groups of times depending on
which machine I landed on.

## What the compiler does alone

The reference code, untouched, at each optimisation level:

| -O0 | -O1 | -O2 | -O3 |
|---|---|---|---|
| 5576 ms | 1583 ms | 1205 ms | 1036 ms |

A factor of 5.4 for one flag, which is the bar the hand work has to clear.

## The seven versions

| version | change | -O0 | -O3 |
|---|---|---|---|
| default | reference | 5576 | 1240 |
| default_nb | borders copied instead of computed | 5312 | 720 |
| optim1 | x loop unrolled | 5140 | 785 |
| optim2 | y loop unrolled | 5155 | 752 |
| optim3 | colour accessors inlined by hand | 2050 | 736 |
| optim4 | 3x3 window rotated along x | 1335 | 654 |
| optim5 | exact borders restored | 1425 | 680 |
| optim6 | column sums reused between pixels | 1115 | 490 |

Times in ms, node az4-a7900-2. The fastest combination is optim6 at `-O3` with
490 ms, which is 11.4x against the untouched code at `-O0` and 2.5x against the
untouched code at `-O3`.

## What I got out of it

The time was not where the code looked worst. Unrolling both loops, which is
the first thing you want to do when you read the kernel, bought 3%. What
mattered was the 37 calls to `ezv_c2r` and friends on every pixel: replacing
them with the shift and mask they perform was worth 2.5x. Those functions are
declared `inline` in the framework header, but gcc ignores that at `-O0`, so
each call paid a whole call sequence to do about two instructions of work.

Some transformations only help at one optimisation level. Unrolling the y loop
does nothing at `-O0`, because the nine pixel values end up on the stack and
the extra loads eat what the loop control saved. The same code is faster from
`-O1`, where those values stay in registers. Hand inlining is the opposite:
2.5x at `-O0`, worthless from `-O1` where the compiler does it itself.

What no optimisation level did for me was reuse data between iterations. Moving
from one pixel to the next, two of the three columns of the window are the ones
just read. Keeping them in variables (optim4) drops the loads from 9 to 3 per
pixel, and keeping their sums rather than the pixels (optim6) drops the
additions from 8 to 4 per channel. Those two pay at every level.

Exact edge cases turned out to be cheap when they stay out of the hot loop. The
reference code tests for borders on all 1,048,576 pixels to get the 4,092 that
actually sit on one. Handling them separately, with the original slow code,
costs 3%.

Measuring once is not enough. Two versions that looked 1.5% apart had
overlapping ranges once I ran each of them three times, and a partition that
mixes two node types gave me bimodal timings until I pinned a node.

## Bonus: the same code on Dalek's four CPUs

| partition | CPU | -O0 optim6 | -O3 optim6 |
|---|---|---|---|
| az4-n4090 | Ryzen 9 7945HX (Zen 4) | 1208 | 526 |
| az4-a7900 | Ryzen 9 7945HX (Zen 4) | 1215 | 523 |
| iml-ia770 | Intel Core Ultra 9 185H | 1165 | 362 |
| az5-a890m | Ryzen AI 9 HX 370 (Zen 5) | 898 | 377 |

The two Zen 4 partitions hold the same CPU and agree to within 1%, which gives
a feel for the measurement error. The two newer laptop chips beat the
desktop-class Zen 4 by about 1.4x here. I would not read much more into it than
that: one run per cell, and both the Core Ultra and the Ryzen AI mix two kinds
of core without SLURM telling me which one ran the job.
